// MithenZipOpen.cpp
// Entry point used by MithenZip.exe, the association target for archive files.
//
//  * plain archive      -> handed to Windows' own archive handler (browses natively)
//  * password protected -> the "Extract files..." window is shown for that archive

#include "StdAfx.h"

#include "ArchiveEngine.h"
#include "ArchiveOps.h"
#include "Dialogs.h"
#include "MithenZip.h"

//Hand the file to Windows' own archive handler (never back to ourselves).
static bool OpenNatively(const std::wstring &archivePath)
{
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

extern "C" __declspec(dllexport) HRESULT __cdecl MithenZip_OpenArchive(LPCWSTR archivePath)
{
  if(!archivePath || !archivePath[0]) {
    return E_INVALIDARG;
  }
  const std::wstring path = archivePath;

  bool needsPassword = false;
  {
    CMithenZipArchive archive;
    const bool opened = archive.Open(path.c_str());
    needsPassword = !opened || archive.IsEncrypted();
  }

  if(!needsPassword) {
    return OpenNatively(path) ? S_OK : E_FAIL;
  }

  //Windows' archive handler cannot decrypt; offer our own extraction instead.
  return MithenZip_ExtractFilesInteractive(MithenZip_PromptOwner(),path);
}
