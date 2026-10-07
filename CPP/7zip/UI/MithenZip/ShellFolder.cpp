// ShellFolder.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "Pidl.h"
#include "EnumIDList.h"
#include "DataObject.h"
#include "ExtractIcon.h"
#include "ShellFolder.h"
#include "Dialogs.h"

static const wchar_t * const kColTitleName     = L"Name";
static const wchar_t * const kColTitleSize     = L"Size";
static const wchar_t * const kColTitlePacked   = L"Packed Size";
static const wchar_t * const kColTitleType     = L"Type";
static const wchar_t * const kColTitleModified = L"Date Modified";

struct CChildEntry
{
  std::wstring Name;
  bool IsDir;
};

//True when the Shell's per-user default handler for this file's extension is
//somebody else (for example Windows' archive folder). In that case the Shell
//reached us under a foreign open verb, and opening an archive there can deadlock
//Explorer, so refuse instead.
static bool DefaultHandlerIsForeign(const std::wstring &filePath)
{
  const size_t slash = filePath.find_last_of(L"\\\\/");
  const size_t dot = filePath.find_last_of(L'.');
  if(dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
    return false;
  }
  const std::wstring extension = filePath.substr(dot);
  const std::wstring base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + extension + L"\\\\";
  const wchar_t * const subs[] = { L"UserChoice",L"UserChoiceLatest" };
  for(size_t i = 0; i < sizeof(subs) / sizeof(subs[0]); i++) {
    HKEY key = NULL;
    if(::RegOpenKeyExW(HKEY_CURRENT_USER,(base + subs[i]).c_str(),0,KEY_READ,&key) == ERROR_SUCCESS) {
      wchar_t value[256];
      DWORD size = sizeof(value);
      DWORD type = 0;
      const LSTATUS status = ::RegQueryValueExW(key,L"ProgId",NULL,&type,(BYTE*)value,&size);
      ::RegCloseKey(key);
      if(status == ERROR_SUCCESS && type == REG_SZ && value[0]) {
        if(wcscmp(value,MITHENZIP_PROGID) != 0) {
          return true;
        }
      }
    }
  }
  return false;
}
static HRESULT SetStrRet(LPSTRRET str,LPCWSTR text)
{
  const size_t len = text ? wcslen(text) : 0;
  LPWSTR buf = (LPWSTR)CoTaskMemAlloc((len + 1) * sizeof(WCHAR));
  if(!buf) {
    return E_OUTOFMEMORY;
  }
  if(len) {
    memcpy(buf,text,len * sizeof(WCHAR));
  }
  buf[len] = 0;
  str->pOleStr = buf;
  str->uType = STRRET_WSTR;
  return S_OK;
}

static std::wstring FormatNumber(UInt64 value)
{
  wchar_t raw[32];
  swprintf_s(raw,32,L"%llu",(unsigned long long)value);
  NUMBERFMTW fmt;
  memset(&fmt,0,sizeof(fmt));
  fmt.Grouping = 3;
  fmt.lpDecimalSep = L".";
  fmt.lpThousandSep = L",";
  fmt.NumDigits = 0;
  wchar_t out[64];
  if(::GetNumberFormatW(LOCALE_USER_DEFAULT,0,raw,&fmt,out,64) > 0) {
    return std::wstring(out);
  }
  return std::wstring(raw);
}

static std::wstring FormatSize(UInt64 size)
{
  UInt64 value = size;
  const wchar_t *unit = L"bytes";
  if(size >= (UInt64)1024 * 1024 * 1024) {
    value = size >> 30;
    unit = L"GB";
  }
  else if(size >= (UInt64)1024 * 1024) {
    value = size >> 20;
    unit = L"MB";
  }
  else if(size >= 1024) {
    value = size >> 10;
    unit = L"KB";
  }
  std::wstring text = FormatNumber(value);
  text += L" ";
  text += unit;
  return text;
}

static std::wstring FormatDateTime(const FILETIME &time)
{
  wchar_t buf[128];
  DWORD flags = FDTF_DEFAULT;
  if(::SHFormatDateTimeW(&time,&flags,buf,128) > 0) {
    return std::wstring(buf);
  }
  return std::wstring();
}

static std::wstring TypeString(const std::wstring &name,bool isDir)
{
  if(isDir) {
    return std::wstring(L"File folder");
  }
  const size_t pos = name.find_last_of(L'.');
  if(pos == std::wstring::npos || pos + 1 >= name.size()) {
    return std::wstring(L"File");
  }
  std::wstring ext = name.substr(pos + 1);
  for(size_t i = 0; i < ext.size(); i++) {
    if(ext[i] >= L'a' && ext[i] <= L'z') {
      ext[i] = (WCHAR)(ext[i] - L'a' + L'A');
    }
  }
  ext += L" File";
  return ext;
}

static void CollectChildren(const std::vector<CMithenZipArcItem> &items,
    const std::wstring &inner,std::vector<CChildEntry> &out)
{
  out.clear();
  const size_t innerLen = inner.size();
  for(size_t i = 0; i < items.size(); i++) {
    const std::wstring &path = items[i].Path;
    if(path.size() <= innerLen) {
      continue;
    }
    if(innerLen && path.compare(0,innerLen,inner) != 0) {
      continue;
    }
    const std::wstring rest = path.substr(innerLen);
    const size_t sep = rest.find(L'\\');
    std::wstring name;
    bool isDir;
    if(sep == std::wstring::npos) {
      name = rest;
      isDir = items[i].IsDir;
    }
    else {
      name = rest.substr(0,sep);
      isDir = true;
    }
    bool found = false;
    for(size_t k = 0; k < out.size(); k++) {
      if(_wcsicmp(out[k].Name.c_str(),name.c_str()) == 0) {
        found = true;
        if(isDir) {
          out[k].IsDir = true;
        }
        break;
      }
    }
    if(!found) {
      CChildEntry entry;
      entry.Name = name;
      entry.IsDir = isDir;
      out.push_back(entry);
    }
  }
}

static bool HasSubItems(const std::vector<CMithenZipArcItem> &items,const std::wstring &prefix)
{
  const size_t len = prefix.size();
  for(size_t i = 0; i < items.size(); i++) {
    const std::wstring &path = items[i].Path;
    if(path.size() > len && path.compare(0,len,prefix) == 0) {
      return true;
    }
  }
  return false;
}

static const CMithenZipArcItem* FindItemByPath(const std::vector<CMithenZipArcItem> &items,
    const std::wstring &fullPath)
{
  for(size_t i = 0; i < items.size(); i++) {
    if(_wcsicmp(items[i].Path.c_str(),fullPath.c_str()) == 0) {
      return &items[i];
    }
  }
  return NULL;
}

CMithenZipFolder::CMithenZipFolder(): _ref(1),_fullPidl(NULL)
{
  ::InterlockedIncrement(&g_dllRefCount);
}

CMithenZipFolder::~CMithenZipFolder()
{
  if(_fullPidl) {
    CoTaskMemFree(_fullPidl);
  }
  ::InterlockedDecrement(&g_dllRefCount);
}

STDMETHODIMP CMithenZipFolder::QueryInterface(REFIID riid,void **ppv)
{
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  if(riid == IID_IUnknown || riid == IID_IShellFolder || riid == IID_IShellFolder2) {
    *ppv = static_cast<IShellFolder*>(this);
  }
  else if(riid == IID_IPersistFolder || riid == IID_IPersistFolder2 || riid == IID_IPersist) {
    *ppv = static_cast<IPersistFolder2*>(this);
  }
  else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) CMithenZipFolder::AddRef()
{
  return (ULONG)::InterlockedIncrement(&_ref);
}

STDMETHODIMP_(ULONG) CMithenZipFolder::Release()
{
  const LONG value = ::InterlockedDecrement(&_ref);
  if(value == 0) {
    delete this;
  }
  return (ULONG)value;
}

STDMETHODIMP CMithenZipFolder::GetClassID(CLSID *pClassID)
{
  if(!pClassID) {
    return E_POINTER;
  }
  *pClassID = CLSID_MithenZipRar;
  return S_OK;
}

STDMETHODIMP CMithenZipFolder::Initialize(PCIDLIST_ABSOLUTE pidl)
{
  COM_TRY_BEGIN
  if(_fullPidl) {
    CoTaskMemFree(_fullPidl);
    _fullPidl = NULL;
  }
  if(pidl) {
    _fullPidl = MithenZip_PidlCopy(pidl);
  }
  wchar_t path[MAX_PATH];
  path[0] = 0;
  bool havePath = (pidl && ::SHGetPathFromIDListW(pidl,path) && path[0]);
  if(!havePath && pidl) {
    //Reached through explorer.exe /e,::{CLSID},<archive>: our pidl carries the path.
    PCUITEMID_CHILD ours = MithenZip_PidlFindLastOurs(pidl);
    if(ours) {
      const wchar_t *name = MithenZip_PidlName(ours);
      if(name && name[0] && ::GetFileAttributesW(name) != INVALID_FILE_ATTRIBUTES) {
        wcsncpy_s(path,MAX_PATH,name,_TRUNCATE);
        havePath = true;
      }
    }
  }
  if(!havePath) {
    //The root folder was opened through "shell:::{CLSID}" (by MithenZip.exe);
    //the launcher recorded which archive to show.
    wchar_t pending[MAX_PATH];
    pending[0] = 0;
    DWORD pendingSize = sizeof(pending);
    DWORD pendingType = 0;
    HKEY pendingKey = NULL;
    if(::RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\MithenZip",0,KEY_READ,&pendingKey) == ERROR_SUCCESS) {
      if(::RegQueryValueExW(pendingKey,L"Pending",NULL,&pendingType,(BYTE*)pending,&pendingSize) == ERROR_SUCCESS &&
          pendingType == REG_SZ && pending[0]) {
        if(::GetFileAttributesW(pending) != INVALID_FILE_ATTRIBUTES) {
          wcsncpy_s(path,MAX_PATH,pending,_TRUNCATE);
          havePath = true;
        }
      }
      ::RegCloseKey(pendingKey);
    }
  }
  //Refuse to be driven by another handler's open verb (see DefaultHandlerIsForeign).
  if(havePath && DefaultHandlerIsForeign(path)) {
    return E_FAIL;
  }
  if(havePath) {
    CMithenZipArchive *archive = new CMithenZipArchive();
    _archive = CMithenZipArchivePtr(archive);
    archive->Release();
    bool opened = _archive->Open(path);
    if(!opened) {
      for(int attempt = 0; attempt < 3; attempt++) {
        std::wstring password;
        if(!MithenZip_ShowPasswordDialog(MithenZip_PromptOwner(),password)) {
          break;
        }
        _archive->SetPassword(password);
        if(_archive->Open(path)) {
          opened = true;
          break;
        }
        ::MessageBoxW(MithenZip_PromptOwner(),L"Incorrect password.",L"MithenZip",MB_OK | MB_ICONWARNING);
      }
    }
    const std::wstring full = path;
    const size_t pos = full.find_last_of(L'\\');
    _displayName = (pos == std::wstring::npos) ? full : full.substr(pos + 1);
    _innerPath.clear();
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::GetCurFolder(PIDLIST_ABSOLUTE *ppidl)
{
  if(!ppidl) {
    return E_POINTER;
  }
  *ppidl = _fullPidl ? MithenZip_PidlCopy(_fullPidl) : NULL;
  return S_OK;
}

STDMETHODIMP CMithenZipFolder::ParseDisplayName(HWND /*hwnd*/,IBindCtx * /*pbc*/,
    LPWSTR pszDisplayName,ULONG *pchEaten,PIDLIST_RELATIVE *ppidl,ULONG *pdwAttributes)
{
  COM_TRY_BEGIN
  if(!ppidl) {
    return E_POINTER;
  }
  *ppidl = NULL;
  if(pchEaten) {
    *pchEaten = 0;
  }
  if(!pszDisplayName) {
    return E_INVALIDARG;
  }
  const std::wstring name = pszDisplayName;
  if(_archive && _wcsicmp(name.c_str(),_archive->FilePath().c_str()) == 0) {
    *ppidl = MithenZip_PidlCreate(NULL,true);
    if(pchEaten) {
      *pchEaten = (ULONG)name.size();
    }
    return S_OK;
  }
  if(!_archive) {
    //The root folder also accepts a filesystem path, which is how
    //"explorer.exe /e,::{CLSID},<archive>" opens an archive.
    if(::GetFileAttributesW(name.c_str()) != INVALID_FILE_ATTRIBUTES) {
      *ppidl = MithenZip_PidlCreate(name.c_str(),false);
      if(!*ppidl) {
        return E_OUTOFMEMORY;
      }
      if(pchEaten) {
        *pchEaten = (ULONG)name.size();
      }
      return S_OK;
    }
    return E_FAIL;
  }
  const std::vector<CMithenZipArcItem> &items = _archive->Items();
  std::wstring inner = _innerPath;
  std::wstring remaining = name;
  ULONG consumed = 0;
  PIDLIST_RELATIVE current = NULL;
  while(!remaining.empty()) {
    const size_t sep = remaining.find(L'\\');
    const std::wstring segment = (sep == std::wstring::npos) ? remaining : remaining.substr(0,sep);
    const std::wstring fullPath = inner + segment;
    const CMithenZipArcItem *item = FindItemByPath(items,fullPath);
    bool isDir = false;
    if(item) {
      isDir = item->IsDir;
    }
    else if(HasSubItems(items,fullPath + L"\\")) {
      isDir = true;
    }
    else {
      break;
    }
    PIDLIST_RELATIVE child = MithenZip_PidlCreate(segment.c_str(),isDir);
    PIDLIST_RELATIVE chain = MithenZip_PidlConcat(current,child);
    if(child) {
      CoTaskMemFree(child);
    }
    if(current) {
      CoTaskMemFree(current);
    }
    current = chain;
    if(!current) {
      return E_OUTOFMEMORY;
    }
    inner += segment;
    inner += L'\\';
    consumed += (ULONG)segment.size();
    if(sep == std::wstring::npos) {
      remaining.clear();
    }
    else {
      remaining = remaining.substr(sep + 1);
      consumed += 1;
    }
  }
  if(!current) {
    return E_FAIL;
  }
  *ppidl = current;
  if(pchEaten) {
    *pchEaten = consumed;
  }
  if(pdwAttributes) {
    SFGAOF flags = *pdwAttributes;
    GetAttributesOf(1,(PCUITEMID_CHILD_ARRAY)&current,&flags);
    *pdwAttributes = flags;
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::EnumObjects(HWND /*hwnd*/,SHCONTF grfFlags,IEnumIDList **ppenum)
{
  COM_TRY_BEGIN
  if(!ppenum) {
    return E_POINTER;
  }
  *ppenum = NULL;
  std::vector<CChildEntry> children;
  if(_archive) {
    CollectChildren(_archive->Items(),_innerPath,children);
  }
  std::vector<LPITEMIDLIST> pidls;
  for(size_t i = 0; i < children.size(); i++) {
    if(children[i].IsDir) {
      if(!(grfFlags & SHCONTF_FOLDERS)) {
        continue;
      }
    }
    else {
      if(!(grfFlags & SHCONTF_NONFOLDERS)) {
        continue;
      }
    }
    LPITEMIDLIST pidl = MithenZip_PidlCreate(children[i].Name.c_str(),children[i].IsDir);
    if(pidl) {
      pidls.push_back(pidl);
    }
  }
  CMithenZipEnumIDList *enumerator = new CMithenZipEnumIDList();
  enumerator->Init(pidls);
  *ppenum = enumerator;
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::BindToObject(PCUIDLIST_RELATIVE pidl,IBindCtx * /*pbc*/,
    REFIID riid,void **ppv)
{
  COM_TRY_BEGIN
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  if(riid != IID_IShellFolder && riid != IID_IShellFolder2) {
    return E_NOINTERFACE;
  }
  if(!pidl || !MithenZip_PidlIsOurs(pidl) || !MithenZip_PidlIsDir(pidl)) {
    return E_INVALIDARG;
  }
  CMithenZipFolder *sub = new CMithenZipFolder();
  sub->_archive = _archive;
  sub->_innerPath = _innerPath;
  sub->_innerPath += MithenZip_PidlName(pidl);
  sub->_innerPath += L'\\';
  sub->_displayName = MithenZip_PidlName(pidl);
  if(_fullPidl) {
    sub->_fullPidl = MithenZip_PidlConcat(_fullPidl,pidl);
  }
  const HRESULT result = sub->QueryInterface(riid,ppv);
  sub->Release();
  return result;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::BindToStorage(PCUIDLIST_RELATIVE /*pidl*/,IBindCtx * /*pbc*/,
    REFIID /*riid*/,void ** /*ppv*/)
{
  return E_NOTIMPL;
}

STDMETHODIMP CMithenZipFolder::CompareIDs(LPARAM /*lParam*/,
    PCUIDLIST_RELATIVE pidl1,PCUIDLIST_RELATIVE pidl2)
{
  COM_TRY_BEGIN
  int result = 0;
  if(!pidl1 || !pidl2) {
    if(pidl1 != pidl2) {
      result = pidl1 ? 1 : -1;
    }
  }
  else if(MithenZip_PidlIsOurs(pidl1) && MithenZip_PidlIsOurs(pidl2)) {
    result = MithenZip_PidlCompare(pidl1,pidl2);
  }
  return (HRESULT)(short)result;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::CreateViewObject(HWND /*hwndOwner*/,REFIID riid,void **ppv)
{
  COM_TRY_BEGIN
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  if(riid == IID_IShellView) {
    SFV_CREATE create;
    create.cbSize = sizeof(create);
    create.pshf = static_cast<IShellFolder*>(this);
    create.psvOuter = NULL;
    create.psfvcb = NULL;
    return ::SHCreateShellFolderView(&create,(IShellView**)ppv);
  }
  return E_NOINTERFACE;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::GetAttributesOf(UINT cidl,PCUITEMID_CHILD_ARRAY apidl,SFGAOF *rgfInOut)
{
  COM_TRY_BEGIN
  if(!rgfInOut) {
    return E_POINTER;
  }
  const SFGAOF requested = *rgfInOut;
  SFGAOF available = 0xFFFFFFFF;
  if(cidl == 0) {
    available = SFGAO_FOLDER | SFGAO_BROWSABLE | SFGAO_CANLINK;
    if(_archive) {
      std::vector<CChildEntry> children;
      CollectChildren(_archive->Items(),_innerPath,children);
      if(!children.empty()) {
        available |= SFGAO_HASSUBFOLDER;
      }
    }
  }
  else {
    for(UINT i = 0; i < cidl; i++) {
      SFGAOF flags = 0;
      if(apidl[i] && MithenZip_PidlIsOurs(apidl[i])) {
        if(MithenZip_PidlIsDir(apidl[i])) {
          flags = SFGAO_FOLDER | SFGAO_BROWSABLE | SFGAO_CANLINK;
          if(_archive) {
            const std::wstring prefix = _innerPath + MithenZip_PidlName(apidl[i]) + L"\\";
            if(HasSubItems(_archive->Items(),prefix)) {
              flags |= SFGAO_HASSUBFOLDER;
            }
          }
        }
        else {
          flags = SFGAO_STREAM | SFGAO_CANCOPY | SFGAO_CANMOVE | SFGAO_CANLINK;
        }
      }
      available &= flags;
    }
  }
  *rgfInOut = requested & available;
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::GetUIObjectOf(HWND /*hwndOwner*/,UINT cidl,
    PCUITEMID_CHILD_ARRAY apidl,REFIID riid,UINT *rgfReserved,void **ppv)
{
  COM_TRY_BEGIN
  if(rgfReserved) {
    *rgfReserved = 0;
  }
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  if(riid == IID_IExtractIconW || riid == IID_IExtractIcon) {
    if(cidl != 1 || !apidl[0] || !MithenZip_PidlIsOurs(apidl[0])) {
      return E_INVALIDARG;
    }
    CMithenZipExtractIcon *icon = new CMithenZipExtractIcon();
    icon->Init(MithenZip_PidlName(apidl[0]),MithenZip_PidlIsDir(apidl[0]));
    *ppv = static_cast<IExtractIconW*>(icon);
    return S_OK;
  }
  if(riid == IID_IDataObject) {
    CMithenZipDataObject *object = new CMithenZipDataObject();
    object->Init(_archive,_innerPath,cidl,apidl);
    *ppv = static_cast<IDataObject*>(object);
    return S_OK;
  }
  return E_NOINTERFACE;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::GetDisplayNameOf(PCUITEMID_CHILD pidl,SHGDNF uFlags,LPSTRRET str)
{
  COM_TRY_BEGIN
  if(!str) {
    return E_POINTER;
  }
  std::wstring name;
  if(!pidl || !MithenZip_PidlIsOurs(pidl)) {
    if((uFlags & SHGDN_FORPARSING) && _archive) {
      name = _archive->FilePath();
    }
    else {
      name = _displayName;
    }
  }
  else {
    if(uFlags & SHGDN_FORPARSING) {
      name = _innerPath;
      name += MithenZip_PidlName(pidl);
    }
    else {
      name = MithenZip_PidlName(pidl);
    }
  }
  return SetStrRet(str,name.c_str());
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::SetNameOf(HWND /*hwnd*/,PCUITEMID_CHILD /*pidl*/,
    LPCWSTR /*pszName*/,SHGDNF /*uFlags*/,PITEMID_CHILD * /*ppidlOut*/)
{
  return E_NOTIMPL;
}

STDMETHODIMP CMithenZipFolder::GetDefaultSearchGUID(GUID * /*pguid*/)
{
  return E_NOTIMPL;
}

STDMETHODIMP CMithenZipFolder::EnumSearches(IEnumExtraSearch ** /*ppenum*/)
{
  return E_NOTIMPL;
}

STDMETHODIMP CMithenZipFolder::GetDefaultColumn(DWORD /*dwReserved*/,ULONG *pSort,ULONG *pDisplay)
{
  if(pSort) {
    *pSort = 0;
  }
  if(pDisplay) {
    *pDisplay = 0;
  }
  return S_OK;
}

STDMETHODIMP CMithenZipFolder::GetDefaultColumnState(UINT iColumn,SHCOLSTATEF *pcsFlags)
{
  if(!pcsFlags) {
    return E_POINTER;
  }
  if(iColumn > 4) {
    return E_INVALIDARG;
  }
  SHCOLSTATEF flags = SHCOLSTATE_ONBYDEFAULT | SHCOLSTATE_TYPE_STR;
  if(iColumn == 4) {
    flags = SHCOLSTATE_ONBYDEFAULT | SHCOLSTATE_TYPE_DATE;
  }
  *pcsFlags = flags;
  return S_OK;
}

STDMETHODIMP CMithenZipFolder::GetDetailsEx(PCUITEMID_CHILD /*pidl*/,
    const SHCOLUMNID * /*pscid*/,VARIANT * /*pv*/)
{
  return E_NOTIMPL;
}

STDMETHODIMP CMithenZipFolder::GetDetailsOf(PCUITEMID_CHILD pidl,UINT iColumn,SHELLDETAILS *psd)
{
  COM_TRY_BEGIN
  if(!psd) {
    return E_POINTER;
  }
  if(!pidl) {
    psd->fmt = LVCFMT_LEFT;
    switch(iColumn) {
      case 0: return SetStrRet(&psd->str,kColTitleName);
      case 1: return SetStrRet(&psd->str,kColTitleSize);
      case 2: return SetStrRet(&psd->str,kColTitlePacked);
      case 3: return SetStrRet(&psd->str,kColTitleType);
      case 4: return SetStrRet(&psd->str,kColTitleModified);
    }
    return S_FALSE;
  }
  if(!MithenZip_PidlIsOurs(pidl)) {
    if(iColumn == 0) {
      psd->fmt = LVCFMT_LEFT;
      return SetStrRet(&psd->str,_displayName.c_str());
    }
    return S_FALSE;
  }
  const std::wstring name = MithenZip_PidlName(pidl);
  const bool isDir = MithenZip_PidlIsDir(pidl);
  const CMithenZipArcItem *item = NULL;
  if(_archive) {
    item = FindItemByPath(_archive->Items(),_innerPath + name);
  }
  switch(iColumn) {
    case 0:
      psd->fmt = LVCFMT_LEFT;
      return SetStrRet(&psd->str,name.c_str());
    case 1:
      psd->fmt = LVCFMT_RIGHT;
      if(isDir) {
        return SetStrRet(&psd->str,L"");
      }
      return SetStrRet(&psd->str,FormatSize(item ? item->Size : 0).c_str());
    case 2:
      psd->fmt = LVCFMT_RIGHT;
      if(isDir) {
        return SetStrRet(&psd->str,L"");
      }
      return SetStrRet(&psd->str,FormatSize(item ? item->PackSize : 0).c_str());
    case 3:
      psd->fmt = LVCFMT_LEFT;
      return SetStrRet(&psd->str,TypeString(name,isDir).c_str());
    case 4:
      psd->fmt = LVCFMT_LEFT;
      if(item && item->HasMTime) {
        return SetStrRet(&psd->str,FormatDateTime(item->MTime).c_str());
      }
      return SetStrRet(&psd->str,L"");
  }
  return S_FALSE;
  COM_TRY_END
}

STDMETHODIMP CMithenZipFolder::MapColumnToSCID(UINT /*iColumn*/,SHCOLUMNID * /*pscid*/)
{
  return E_NOTIMPL;
}
