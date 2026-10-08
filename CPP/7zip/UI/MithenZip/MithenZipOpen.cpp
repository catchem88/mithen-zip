// MithenZipOpen.cpp
// Entry point used by MithenZip.exe, the association target for archive files.
//
// Extension first:
//   * image types (.iso/.img/.vhd/.vhdx) - Windows mounts them, so hand off at once
//   * types the Windows archive handlers can browse - ask the window the user is
//     already in to browse the archive (no extra window), then fall back to the
//     normal hand-off if that is not possible
//   * anything else we support, or an encrypted archive - our Extract window

#include "StdAfx.h"

#include <exdisp.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "ArchiveEngine.h"
#include "ArchiveOps.h"
#include "Dialogs.h"
#include "MithenZip.h"


static std::wstring ExtensionOf(const std::wstring &path)
{
  const size_t dot = path.find_last_of(L'.');
  const size_t slash = path.find_last_of(L"\\/");
  if(dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
    return std::wstring();
  }
  return path.substr(dot);
}

static std::wstring ParentDir(const std::wstring &path)
{
  const size_t slash = path.find_last_of(L"\\/");
  if(slash == std::wstring::npos) {
    return std::wstring();
  }
  return path.substr(0,slash);
}

//Does this shell window show the folder the archive lives in?
static bool WindowShowsFolder(IWebBrowser2 *browser,const std::wstring &folder)
{
  if(folder.empty()) {
    return false;
  }
  BSTR location = NULL;
  if(browser->get_LocationURL(&location) != S_OK || !location) {
    return false;
  }
  wchar_t localPath[MAX_PATH];
  localPath[0] = 0;
  DWORD length = MAX_PATH;
  if(::PathCreateFromUrlW(location,localPath,&length,0) != S_OK) {
    localPath[0] = 0;
  }
  ::SysFreeString(location);
  if(!localPath[0]) {
    return false;
  }
  return (_wcsicmp(localPath,folder.c_str()) == 0);
}

//Ask the Explorer window the user opened this from to browse the archive, so it is
//shown in that window instead of a new one.
static bool BrowseExistingWindow(const std::wstring &archivePath)
{
  const HWND foreground = ::GetForegroundWindow();
  const HWND foregroundRoot = foreground ? ::GetAncestor(foreground,GA_ROOT) : NULL;
  const std::wstring folder = ParentDir(archivePath);
  if(!foreground || !foregroundRoot) {
    return false;
  }

  IShellWindows *shellWindows = NULL;
  if(::CoCreateInstance(CLSID_ShellWindows,NULL,CLSCTX_ALL,
      IID_PPV_ARGS(&shellWindows)) != S_OK || !shellWindows) {
    return false;
  }

  PIDLIST_ABSOLUTE pidl = NULL;
  const HRESULT parseResult = ::SHParseDisplayName(archivePath.c_str(),NULL,&pidl,0,NULL);
  if(parseResult != S_OK || !pidl) {
    shellWindows->Release();
    return false;
  }

  bool browsed = false;
  long count = 0;
  if(SUCCEEDED(shellWindows->get_Count(&count))) {
    for(long i = 0; i < count && !browsed; i++) {
      VARIANT index;
      ::VariantInit(&index);
      index.vt = VT_I4;
      index.lVal = i;
      IDispatch *dispatch = NULL;
      if(shellWindows->Item(index,&dispatch) == S_OK && dispatch) {
        IServiceProvider *provider = NULL;
        if(dispatch->QueryInterface(IID_PPV_ARGS(&provider)) == S_OK && provider) {
          IShellBrowser *browser = NULL;
          if(provider->QueryService(SID_STopLevelBrowser,IID_PPV_ARGS(&browser)) == S_OK && browser) {
            HWND handle = NULL;
            if(browser->GetWindow(&handle) == S_OK) {
              bool matches = (handle == foreground) || (handle == foregroundRoot);
              if(!matches) {
                IWebBrowser2 *webBrowser = NULL;
                if(dispatch->QueryInterface(IID_PPV_ARGS(&webBrowser)) == S_OK && webBrowser) {
                  matches = WindowShowsFolder(webBrowser,folder);
                  webBrowser->Release();
                }
              }
              if(matches) {
                const HRESULT hr = browser->BrowseObject(pidl,SBSP_ABSOLUTE);
                browsed = (hr == S_OK);
              }
            }
            browser->Release();
          }
          provider->Release();
        }
        dispatch->Release();
      }
      ::VariantClear(&index);
    }
  }
  ::CoTaskMemFree(pidl);
  shellWindows->Release();
  return browsed;
}

//Hand the file to the Windows handler: the archive folder handlers when asked for,
//otherwise whatever Windows uses for the type by default (mounting an image).
static bool OpenNatively(const std::wstring &archivePath,bool archiveHandlers)
{
  if(archiveHandlers) {
    if(BrowseExistingWindow(archivePath)) {
      return true;
    }
    const wchar_t * const classNames[] = { L"CompressedFolder",L"ArchiveFolder" };
    for(size_t i = 0; i < sizeof(classNames) / sizeof(classNames[0]); i++) {
      SHELLEXECUTEINFOW info;
      memset(&info,0,sizeof(info));
      info.cbSize = sizeof(info);
      info.fMask = SEE_MASK_CLASSNAME | SEE_MASK_FLAG_NO_UI;
      info.lpVerb = L"open";
      info.lpFile = archivePath.c_str();
      info.lpClass = classNames[i];
      info.nShow = SW_SHOWNORMAL;
      if(::ShellExecuteExW(&info)) {
        return true;
      }
    }
    return false;
  }
  const HINSTANCE result = ::ShellExecuteW(NULL,L"open",archivePath.c_str(),NULL,NULL,SW_SHOWNORMAL);
  return ((INT_PTR)result > 32);
}

extern "C" __declspec(dllexport) HRESULT __cdecl MithenZip_OpenArchive(LPCWSTR archivePath)
{
  if(!archivePath || !archivePath[0]) {
    return E_INVALIDARG;
  }
  const std::wstring path = archivePath;
  const std::wstring extension = ExtensionOf(path);

  const HRESULT comResult = ::CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);

  HRESULT result = E_FAIL;
  //Images are mounted by Windows; no probing, so a large image never stalls.
  if(MithenZip_IsImageExtension(extension)) {
    result = OpenNatively(path,false) ? S_OK : E_FAIL;
  }
  else {
    bool needsPassword = false;
    {
      CMithenZipArchive archive;
      const bool opened = archive.Open(path.c_str());
      needsPassword = !opened || archive.IsEncrypted();
    }
    if(!needsPassword && MithenZip_IsWindowsOpenableExtension(extension) && OpenNatively(path,true)) {
      result = S_OK;
    }
    else {
      //Encrypted, or no Windows browser for this format: offer our own extraction.
      result = MithenZip_ExtractFilesInteractive(MithenZip_PromptOwner(),path);
    }
  }

  if(SUCCEEDED(comResult)) {
    ::CoUninitialize();
  }
  return result;
}
