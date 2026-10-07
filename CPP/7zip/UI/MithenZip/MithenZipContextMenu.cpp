// ContextMenu.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "MithenZip.h"
#include "ArchiveEngine.h"
#include "ArchiveOps.h"
#include "Dialogs.h"
#include "MithenZipContextMenu.h"

static const wchar_t * const kSubMenuTitle = L"Archive";

static const wchar_t * const kVerbs[] = {
  L"AddToArchive",L"AddToZip",L"AddTo7z",
  L"OpenArchive",L"ExtractFiles",L"ExtractHere"
};


static std::wstring FileExtensionLower(const std::wstring &path)
{
  const std::wstring name = MithenZip_FileName(path);
  const size_t dot = name.find_last_of(L'.');
  if(dot == std::wstring::npos || dot + 1 >= name.size()) {
    return std::wstring();
  }
  std::wstring ext = name.substr(dot);
  for(size_t i = 0; i < ext.size(); i++) {
    if(ext[i] >= L'A' && ext[i] <= L'Z') {
      ext[i] = (WCHAR)(ext[i] - L'A' + L'a');
    }
  }
  return ext;
}

static bool GetSelectedPaths(IDataObject *dataObject,std::vector<std::wstring> &paths)
{
  paths.clear();
  if(!dataObject) {
    return false;
  }
  FORMATETC format;
  memset(&format,0,sizeof(format));
  format.cfFormat = CF_HDROP;
  format.dwAspect = DVASPECT_CONTENT;
  format.lindex = -1;
  format.tymed = TYMED_HGLOBAL;
  STGMEDIUM medium;
  memset(&medium,0,sizeof(medium));
  if(dataObject->GetData(&format,&medium) != S_OK) {
    return false;
  }
  bool ok = false;
  if(medium.tymed == TYMED_HGLOBAL && medium.hGlobal) {
    HDROP drop = (HDROP)medium.hGlobal;
    const UINT count = ::DragQueryFileW(drop,0xFFFFFFFF,NULL,0);
    for(UINT i = 0; i < count; i++) {
      const UINT length = ::DragQueryFileW(drop,i,NULL,0);
      std::wstring path;
      path.resize((size_t)length + 1);
      ::DragQueryFileW(drop,i,&path[0],length + 1);
      path.resize((size_t)length);
      if(!path.empty()) {
        paths.push_back(path);
      }
    }
    ok = !paths.empty();
  }
  ::ReleaseStgMedium(&medium);
  return ok;
}

static std::wstring MakeDefaultArchivePath(const std::vector<std::wstring> &files,const wchar_t *extension)
{
  std::wstring base = MithenZip_SelectionBaseName(files);
  for(size_t i = 0; i < files.size(); i++) {
    const std::wstring fileName = MithenZip_FileName(files[i]);
    if(_wcsicmp(fileName.c_str(),(base + extension).c_str()) == 0) {
      base += L"_";
      break;
    }
  }
  return MithenZip_ParentDir(files[0]) + base + extension;
}

static void ReportResult(HWND owner,HRESULT result)
{
  if(result == S_OK || result == E_ABORT) {
    return;
  }
  wchar_t suppress[8];
  if(::GetEnvironmentVariableW(L"MITHENZIP_NO_UI",suppress,8) != 0) {
    return;
  }
  if(result == MITHENZIP_E_WRONG_PASSWORD) {
    ::MessageBoxW(owner,L"The password is incorrect. Extraction stopped.",L"MithenZip",MB_OK | MB_ICONERROR);
    return;
  }
  wchar_t buffer[256];
  swprintf_s(buffer,256,L"The operation failed (0x%08lX).",(unsigned long)result);
  ::MessageBoxW(owner,buffer,L"MithenZip",MB_OK | MB_ICONERROR);
}

static HRESULT CompressWithProgress(HWND owner,const std::vector<std::wstring> &files,
    const std::wstring &archivePath,const CMithenZipCompressOptions &options)
{
  CMithenZipProgressDialog progress;
  progress.Create(owner,L"Adding to archive - MithenZip");
  const HRESULT result = MithenZip_Compress(&progress,files,archivePath,options);
  progress.Destroy();
  return result;
}

static HRESULT ExtractWithProgress(HWND owner,const std::wstring &archivePath,
    const CMithenZipExtractOptions &options)
{
  CMithenZipProgressDialog progress;
  progress.Create(owner,L"Extracting - MithenZip");
  const HRESULT result = MithenZip_Extract(&progress,archivePath,options);
  progress.Destroy();
  return result;
}


CMithenZipContextMenu::CMithenZipContextMenu():
    _ref(1),_idCmdFirst(0),_hasDirectories(false),_hasArchive(false)
{
  ::InterlockedIncrement(&g_dllRefCount);
}

CMithenZipContextMenu::~CMithenZipContextMenu()
{
  ::InterlockedDecrement(&g_dllRefCount);
}

STDMETHODIMP CMithenZipContextMenu::QueryInterface(REFIID riid,void **ppv)
{
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  if(riid == IID_IUnknown || riid == IID_IContextMenu) {
    *ppv = static_cast<IContextMenu*>(this);
    AddRef();
    return S_OK;
  }
  if(riid == IID_IShellExtInit) {
    *ppv = static_cast<IShellExtInit*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CMithenZipContextMenu::AddRef()
{
  return (ULONG)::InterlockedIncrement(&_ref);
}

STDMETHODIMP_(ULONG) CMithenZipContextMenu::Release()
{
  const LONG value = ::InterlockedDecrement(&_ref);
  if(value == 0) {
    delete this;
  }
  return (ULONG)value;
}

STDMETHODIMP CMithenZipContextMenu::Initialize(LPCITEMIDLIST /*pidlFolder*/,
    LPDATAOBJECT dataObject,HKEY /*hkeyProgID*/)
{
  COM_TRY_BEGIN
  _fileNames.clear();
  _commands.clear();
  _hasDirectories = false;
  _hasArchive = false;

  if(!dataObject) {
    return E_INVALIDARG;
  }
  GetSelectedPaths(dataObject,_fileNames);

  for(size_t i = 0; i < _fileNames.size(); i++) {
    const DWORD attributes = ::GetFileAttributesW(_fileNames[i].c_str());
    if(attributes == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    if(attributes & FILE_ATTRIBUTE_DIRECTORY) {
      _hasDirectories = true;
    }
    else {
      const std::wstring extension = FileExtensionLower(_fileNames[i]);
      if(!extension.empty() && MithenZip_IsArchiveExtension(extension)) {
        _hasArchive = true;
      }
    }
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CMithenZipContextMenu::QueryContextMenu(HMENU hMenu,UINT indexMenu,
    UINT idCmdFirst,UINT idCmdLast,UINT uFlags)
{
  COM_TRY_BEGIN
  _commands.clear();
  _idCmdFirst = idCmdFirst;

  if(_fileNames.empty()) {
    return MAKE_HRESULT(SEVERITY_SUCCESS,0,0);
  }
  if((uFlags & 0x000F) != CMF_NORMAL &&
      (uFlags & CMF_VERBSONLY) == 0 &&
      (uFlags & CMF_EXPLORE) == 0) {
    return MAKE_HRESULT(SEVERITY_SUCCESS,0,0);
  }

  const std::wstring baseName = MithenZip_SelectionBaseName(_fileNames);
  const bool showArchiveCommands = _hasArchive && !_hasDirectories;

  HMENU popup = ::CreatePopupMenu();
  if(!popup) {
    return E_OUTOFMEMORY;
  }

  UINT nextId = idCmdFirst;
  UINT position = 0;

  //The popup itself consumes one id so that ids and the command map stay aligned.
  _commands.push_back(kMithenZipCmd_Popup);
  MENUITEMINFOW item;
  memset(&item,0,sizeof(item));
  item.cbSize = sizeof(item);
  item.fMask = MIIM_TYPE | MIIM_SUBMENU | MIIM_ID;
  item.fType = MFT_STRING;
  item.wID = nextId++;
  item.dwTypeData = (LPWSTR)kSubMenuTitle;
  item.hSubMenu = popup;
  ::InsertMenuItemW(hMenu,indexMenu,TRUE,&item);

  const struct { int Command; const wchar_t *Text; } items[] = {
    { kMithenZipCmd_AddToArchive,L"Add to archive..." },
    { kMithenZipCmd_AddToZip,NULL },
    { kMithenZipCmd_AddTo7z,NULL }
  };

  for(size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
    if(nextId > idCmdLast) {
      break;
    }
    std::wstring text;
    if(items[i].Text) {
      text = items[i].Text;
    }
    else if(items[i].Command == kMithenZipCmd_AddToZip) {
      text = std::wstring(L"Add to \"") + baseName + L".zip\"";
    }
    else {
      text = std::wstring(L"Add to \"") + baseName + L".7z\"";
    }
    MENUITEMINFOW sub;
    memset(&sub,0,sizeof(sub));
    sub.cbSize = sizeof(sub);
    sub.fMask = MIIM_TYPE | MIIM_ID;
    sub.fType = MFT_STRING;
    sub.wID = nextId++;
    sub.dwTypeData = &text[0];
    ::InsertMenuItemW(popup,position++,TRUE,&sub);
    _commands.push_back(items[i].Command);
  }

  if(showArchiveCommands && nextId <= idCmdLast) {
    MENUITEMINFOW separator;
    memset(&separator,0,sizeof(separator));
    separator.cbSize = sizeof(separator);
    separator.fMask = MIIM_TYPE;
    separator.fType = MFT_SEPARATOR;
    ::InsertMenuItemW(popup,position++,TRUE,&separator);

    const struct { int Command; const wchar_t *Text; } archiveItems[] = {
      { kMithenZipCmd_Open,L"Open archive" },
      { kMithenZipCmd_ExtractFiles,L"Extract files..." },
      { kMithenZipCmd_ExtractHere,L"Extract here" }
    };
    for(size_t i = 0; i < sizeof(archiveItems) / sizeof(archiveItems[0]); i++) {
      if(nextId > idCmdLast) {
        break;
      }
      MENUITEMINFOW sub;
      memset(&sub,0,sizeof(sub));
      sub.cbSize = sizeof(sub);
      sub.fMask = MIIM_TYPE | MIIM_ID;
      sub.fType = MFT_STRING;
      sub.wID = nextId++;
      sub.dwTypeData = (LPWSTR)archiveItems[i].Text;
      ::InsertMenuItemW(popup,position++,TRUE,&sub);
      _commands.push_back(archiveItems[i].Command);
    }
  }

  return MAKE_HRESULT(SEVERITY_SUCCESS,0,(USHORT)_commands.size());
  COM_TRY_END
}

static int CommandFromVerb(const wchar_t *verb)
{
  if(!verb) {
    return -1;
  }
  for(int i = 0; i <= (int)kMithenZipCmd_ExtractHere; i++) {
    if(_wcsicmp(verb,kVerbs[i]) == 0) {
      return i;
    }
  }
  return -1;
}

STDMETHODIMP CMithenZipContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
  COM_TRY_BEGIN
  if(!pici) {
    return E_INVALIDARG;
  }
  int command = -1;
  if(HIWORD(pici->lpVerb) == 0) {
    UINT offset = LOWORD(pici->lpVerb);
    if(offset >= _commands.size() && offset >= _idCmdFirst) {
      offset -= _idCmdFirst;
    }
    if(offset < _commands.size()) {
      command = _commands[offset];
    }
  }
  else if(pici->cbSize == sizeof(CMINVOKECOMMANDINFOEX) && (pici->fMask & CMIC_MASK_UNICODE) != 0) {
    const CMINVOKECOMMANDINFOEX *ex = (const CMINVOKECOMMANDINFOEX*)pici;
    command = CommandFromVerb(ex->lpVerbW);
  }
  else {
    wchar_t verbW[128];
    verbW[0] = 0;
    ::MultiByteToWideChar(CP_ACP,0,(LPCSTR)pici->lpVerb,-1,verbW,128);
    command = CommandFromVerb(verbW);
  }
  if(command < 0 || command == kMithenZipCmd_Popup) {
    return E_INVALIDARG;
  }
  return InvokeCommandCommon(command);
  COM_TRY_END
}

HRESULT CMithenZipContextMenu::InvokeCommandCommon(int command)
{
  COM_TRY_BEGIN
  if(_fileNames.empty()) {
    return E_INVALIDARG;
  }
  const HWND owner = ::GetForegroundWindow();

  switch(command) {
    case kMithenZipCmd_AddToArchive:
    {
      CMithenZipCompressOptions options;
      options.FormatName = L"zip";
      std::wstring archivePath = MakeDefaultArchivePath(_fileNames,L".zip");
      if(!MithenZip_ShowAddDialog(owner,_fileNames,archivePath,options)) {
        return S_OK;
      }
      if(archivePath.empty()) {
        return S_OK;
      }
      const HRESULT result = CompressWithProgress(owner,_fileNames,archivePath,options);
      ReportResult(owner,result);
      return S_OK;
    }
    case kMithenZipCmd_AddToZip:
    case kMithenZipCmd_AddTo7z:
    {
      const bool toZip = (command == kMithenZipCmd_AddToZip);
      CMithenZipCompressOptions options;
      options.FormatName = toZip ? L"zip" : L"7z";
      const std::wstring archivePath = MakeDefaultArchivePath(_fileNames,toZip ? L".zip" : L".7z");
      const HRESULT result = CompressWithProgress(owner,_fileNames,archivePath,options);
      ReportResult(owner,result);
      return S_OK;
    }
    case kMithenZipCmd_Open:
    {
      const HINSTANCE instance = ::ShellExecuteW(owner,L"open",_fileNames[0].c_str(),NULL,NULL,SW_SHOWNORMAL);
      if((INT_PTR)instance <= 32) {
        std::wstring parameters = L"/e,\"" + _fileNames[0] + L"\"";
        ::ShellExecuteW(owner,L"open",L"explorer.exe",parameters.c_str(),NULL,SW_SHOWNORMAL);
      }
      return S_OK;
    }
    case kMithenZipCmd_ExtractFiles:
      MithenZip_ExtractFilesInteractive(owner,_fileNames[0]);
      return S_OK;
    case kMithenZipCmd_ExtractHere:
    {
      CMithenZipExtractOptions options;
      options.OutputDir = MithenZip_ParentDir(_fileNames[0]);
      if(options.OutputDir.empty()) {
        return S_OK;
      }
      const HRESULT result = ExtractWithProgress(owner,_fileNames[0],options);
      ReportResult(owner,result);
      return S_OK;
    }
    default:
      break;
  }
  return S_OK;
  COM_TRY_END
}

//Shared with MithenZip.exe's open handler: show the Extract files dialog for a
//single archive and run the extraction.
HRESULT MithenZip_ExtractFilesInteractive(HWND owner,const std::wstring &archivePath)
{
  if(archivePath.empty()) {
    return E_INVALIDARG;
  }
  CMithenZipExtractOptions options;
  const std::wstring dir = MithenZip_ParentDir(archivePath);
  options.OutputDir = dir + MithenZip_RemoveExtension(MithenZip_FileName(archivePath));
  if(!MithenZip_ShowExtractDialog(owner,options)) {
    return E_ABORT;
  }
  if(options.OutputDir.empty()) {
    return E_ABORT;
  }
  const HRESULT result = ExtractWithProgress(owner,archivePath,options);
  ReportResult(owner,result);
  return result;
}

STDMETHODIMP CMithenZipContextMenu::GetCommandString(UINT_PTR idCmd,UINT uType,
    UINT * /*pwReserved*/,LPSTR pszName,UINT cchMax)
{
  if((uType | GCS_UNICODE) == GCS_VALIDATEW) {
    return S_OK;
  }
  UINT offset = (UINT)idCmd;
  if(offset >= _commands.size() && offset >= _idCmdFirst) {
    offset -= _idCmdFirst;
  }
  if(offset >= _commands.size()) {
    return E_INVALIDARG;
  }
  const int command = _commands[offset];
  if(command < 0) {
    return E_INVALIDARG;
  }
  if(cchMax == 0 || !pszName) {
    return S_OK;
  }
  const wchar_t *verb = kVerbs[command];
  if(uType & GCS_UNICODE) {
    wcsncpy_s((wchar_t*)pszName,cchMax,verb,_TRUNCATE);
  }
  else {
    WideCharToMultiByte(CP_ACP,0,verb,-1,pszName,(int)cchMax,NULL,NULL);
  }
  return S_OK;
}
