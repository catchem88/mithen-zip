// MithenZipDll.cpp

#include "StdAfx.h"

#include "../../../Common/MyInitGuid.h"

#include "MithenZip.h"
#include "ShellFolder.h"
#include "MithenZipContextMenu.h"
#include "ArchiveEngine.h"

#include <shobjidl.h>

HINSTANCE g_hInstance = NULL;
LONG g_dllRefCount = 0;

static const wchar_t * const kClassesPrefix = L"Software\\Classes\\";
static const wchar_t * const kSoftwareKey   = L"Software\\MithenZip";
static const wchar_t * const kBackupSubKey  = L"Software\\MithenZip\\Backup";
static const wchar_t * const kRootValue     = L"ClassesRoot";
static const wchar_t * const kApproved      =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";



//Shell extension handler registrations (the prefix is one of * / Folder / Directory).
static const wchar_t * const kShellExPrefixes[] = { L"*",L"Folder",L"Directory" };


static bool WriteString(HKEY root,LPCWSTR subKey,LPCWSTR name,LPCWSTR value)
{
  HKEY key = NULL;
  if(::RegCreateKeyExW(root,subKey,0,NULL,REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&key,NULL) != ERROR_SUCCESS) {
    return false;
  }
  const DWORD size = (DWORD)((wcslen(value) + 1) * sizeof(WCHAR));
  const LONG result = ::RegSetValueExW(key,name,0,REG_SZ,(const BYTE*)value,size);
  ::RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

static bool WriteDword(HKEY root,LPCWSTR subKey,LPCWSTR name,DWORD value)
{
  HKEY key = NULL;
  if(::RegCreateKeyExW(root,subKey,0,NULL,REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&key,NULL) != ERROR_SUCCESS) {
    return false;
  }
  const LONG result = ::RegSetValueExW(key,name,0,REG_DWORD,(const BYTE*)&value,sizeof(value));
  ::RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

static bool ReadString(HKEY root,LPCWSTR subKey,LPCWSTR name,std::wstring &value)
{
  HKEY key = NULL;
  if(::RegOpenKeyExW(root,subKey,0,KEY_READ,&key) != ERROR_SUCCESS) {
    return false;
  }
  wchar_t buffer[1024];
  DWORD size = sizeof(buffer);
  DWORD type = 0;
  const LONG result = ::RegQueryValueExW(key,name,NULL,&type,(BYTE*)buffer,&size);
  ::RegCloseKey(key);
  if(result != ERROR_SUCCESS || type != REG_SZ) {
    return false;
  }
  value.assign(buffer);
  return true;
}

static void DeleteDefaultValue(HKEY root,LPCWSTR subKey)
{
  HKEY key = NULL;
  if(::RegOpenKeyExW(root,subKey,0,KEY_SET_VALUE,&key) != ERROR_SUCCESS) {
    return;
  }
  ::RegDeleteValueW(key,NULL);
  ::RegCloseKey(key);
}

static void WriteApproved(LPCWSTR clsid,LPCWSTR name)
{
  HKEY key = NULL;
  if(::RegCreateKeyExW(HKEY_LOCAL_MACHINE,kApproved,0,NULL,REG_OPTION_NON_VOLATILE,
      KEY_WRITE,NULL,&key,NULL) != ERROR_SUCCESS) {
    return;
  }
  ::RegSetValueExW(key,clsid,0,REG_SZ,(const BYTE*)name,(DWORD)((wcslen(name) + 1) * sizeof(WCHAR)));
  ::RegCloseKey(key);
}

static void DeleteApproved(LPCWSTR clsid)
{
  HKEY key = NULL;
  if(::RegOpenKeyExW(HKEY_LOCAL_MACHINE,kApproved,0,KEY_SET_VALUE,&key) != ERROR_SUCCESS) {
    return;
  }
  ::RegDeleteValueW(key,clsid);
  ::RegCloseKey(key);
}

//Machine-wide registration needs administrator rights. When that is not
//available, fall back to a per-user registration under HKCU\Software\Classes.
static HKEY ResolveClassesRoot()
{
  HKEY key = NULL;
  if(::RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"Software\\Classes\\CLSID",0,NULL,
      REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&key,NULL) == ERROR_SUCCESS) {
    ::RegCloseKey(key);
    return HKEY_LOCAL_MACHINE;
  }
  return HKEY_CURRENT_USER;
}


class CMithenZipClassFactory Z7_final: public IClassFactory
{
  LONG _ref;
  bool _contextMenu;

public:
  CMithenZipClassFactory(bool contextMenu):
      _ref(1),_contextMenu(contextMenu)
  {
    ::InterlockedIncrement(&g_dllRefCount);
  }

  ~CMithenZipClassFactory() { ::InterlockedDecrement(&g_dllRefCount); }

  STDMETHOD(QueryInterface)(REFIID riid,void **ppv)
  {
    if(!ppv) {
      return E_POINTER;
    }
    *ppv = NULL;
    if(riid == IID_IUnknown || riid == IID_IClassFactory) {
      *ppv = static_cast<IClassFactory*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHOD_(ULONG,AddRef)()
  {
    return (ULONG)::InterlockedIncrement(&_ref);
  }

  STDMETHOD_(ULONG,Release)()
  {
    const LONG value = ::InterlockedDecrement(&_ref);
    if(value == 0) {
      delete this;
    }
    return (ULONG)value;
  }

  STDMETHOD(CreateInstance)(IUnknown *pUnkOuter,REFIID riid,void **ppv)
  {
    if(!ppv) {
      return E_POINTER;
    }
    *ppv = NULL;
    if(pUnkOuter) {
      return CLASS_E_NOAGGREGATION;
    }
    if(_contextMenu) {
      CMithenZipContextMenu *menu = new CMithenZipContextMenu();
      const HRESULT result = menu->QueryInterface(riid,ppv);
      menu->Release();
      return result;
    }
    CMithenZipFolder *folder = new CMithenZipFolder();
    const HRESULT result = folder->QueryInterface(riid,ppv);
    folder->Release();
    return result;
  }

  STDMETHOD(LockServer)(BOOL /*fLock*/)
  {
    return S_OK;
  }
};


extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance,DWORD dwReason,LPVOID /*reserved*/)
{
  if(dwReason == DLL_PROCESS_ATTACH) {
    g_hInstance = hInstance;
  }
  return TRUE;
}

STDAPI DllCanUnloadNow()
{
  return (g_dllRefCount == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid,REFIID riid,LPVOID *ppv)
{
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  bool contextMenu = false;
  if(IsEqualGUID(rclsid,CLSID_MithenZipRar)) {
    contextMenu = false;
  }
  else if(IsEqualGUID(rclsid,CLSID_MithenZipContextMenu)) {
    contextMenu = true;
  }
  else {
    return CLASS_E_CLASSNOTAVAILABLE;
  }
  CMithenZipClassFactory *factory = new CMithenZipClassFactory(contextMenu);
  const HRESULT result = factory->QueryInterface(riid,ppv);
  factory->Release();
  return result;
}

static bool RegisterServerInRoot(HKEY root,bool associate)
{
  wchar_t modulePath[MAX_PATH];
  if(!::GetModuleFileNameW(g_hInstance,modulePath,MAX_PATH)) {
    return false;
  }
  const std::wstring iconPath = std::wstring(modulePath) + L",-1";
  std::wstring modulePathDir = modulePath;
  const size_t lastSlash = modulePathDir.find_last_of(L"\\\\");
  if(lastSlash != std::wstring::npos) {
    modulePathDir.erase(lastSlash);
  }
  wchar_t systemRoot[MAX_PATH];
  std::wstring archiveIcon = L"C:\\Windows\\system32\\zipfldr.dll";
  if(::GetEnvironmentVariableW(L"SystemRoot",systemRoot,MAX_PATH) && systemRoot[0]) {
    archiveIcon = std::wstring(systemRoot) + L"\\system32\\zipfldr.dll";
  }

  //Shell folder (StorageHandler)
  const std::wstring clsidKey = std::wstring(kClassesPrefix) + L"CLSID\\" + MITHENZIP_CLSID_STR;
  if(!WriteString(root,clsidKey.c_str(),NULL,MITHENZIP_FRIENDLY_NAME)) {
    return false;
  }
  if(!WriteString(root,(clsidKey + L"\\DefaultIcon").c_str(),NULL,iconPath.c_str())) {
    return false;
  }
  if(!WriteString(root,(clsidKey + L"\\InprocServer32").c_str(),NULL,modulePath)) {
    return false;
  }
  if(!WriteString(root,(clsidKey + L"\\InprocServer32").c_str(),L"ThreadingModel",L"Apartment")) {
    return false;
  }
  if(!WriteDword(root,(clsidKey + L"\\ShellFolder").c_str(),L"Attributes",0xA0000000u)) {
    return false;
  }
  if(!WriteString(root,(clsidKey + L"\\ProgID").c_str(),NULL,MITHENZIP_PROGID)) {
    return false;
  }
  //The Shell only treats a CLSID as a folder handler when it declares the shell folder category.
  if(!WriteString(root,
      (clsidKey + L"\\Implemented Categories\\{00021490-0000-0000-C000-000000000046}").c_str(),
      NULL,L"")) {
    return false;
  }

  const std::wstring progIdKey = std::wstring(kClassesPrefix) + MITHENZIP_PROGID;
  if(!WriteString(root,progIdKey.c_str(),NULL,MITHENZIP_FRIENDLY_NAME)) {
    return false;
  }
  //Use the same icon as the built-in archive folder.
  if(!WriteString(root,(progIdKey + L"\\DefaultIcon").c_str(),NULL,archiveIcon.c_str())) {
    return false;
  }
  //Name shown in the "Open with" list.
  if(!WriteString(root,progIdKey.c_str(),L"FriendlyTypeName",
      (std::wstring(L"@") + modulePath + L",-101").c_str())) {
    return false;
  }
  if(!WriteString(root,progIdKey.c_str(),L"AppUserModelID",L"Microsoft.Windows.Explorer")) {
    return false;
  }

  std::wstring installDir = modulePath;
  const size_t dirSlash = installDir.find_last_of(L"\\/");
  if(dirSlash != std::wstring::npos) {
    installDir.erase(dirSlash);
  }
  const std::wstring launcherPath = installDir + L"\\MithenZip.exe";

  //The association target is MithenZip.exe; the exe decides between native
  //browsing and our own extraction window.
  if(!WriteString(root,(progIdKey + L"\\shell\\open\\command").c_str(),NULL,
      (std::wstring(L"\"") + launcherPath + L"\" \"%1\"").c_str())) {
    return false;
  }
  //Remove the StorageHandler written by earlier builds: while it is present the
  //Shell treats archives as folders and ignores our open command.
  {
    const std::wstring storageKey = progIdKey + L"\\ShellEx\\StorageHandler";
    std::wstring handler;
    if(ReadString(root,storageKey.c_str(),NULL,handler) && handler == MITHENZIP_CLSID_STR) {
      ::SHDeleteKeyW(root,storageKey.c_str());
    }
  }

  //Register as a normal application so Windows can list and brand us.
  const std::wstring appKey = std::wstring(kClassesPrefix) + L"Applications\\MithenZip.exe";
  if(!WriteString(root,appKey.c_str(),L"FriendlyAppName",L"MithenZip")) {
    return false;
  }
  if(!WriteString(root,(appKey + L"\\DefaultIcon").c_str(),NULL,
      (launcherPath + L",0").c_str())) {
    return false;
  }
  if(!WriteString(root,(appKey + L"\\shell\\open\\command").c_str(),NULL,
      (std::wstring(L"\"") + launcherPath + L"\" \"%1\"").c_str())) {
    return false;
  }

  //Context menu handler
  const std::wstring ctxKey = std::wstring(kClassesPrefix) + L"CLSID\\" + MITHENZIP_CTX_CLSID_STR;
  if(!WriteString(root,ctxKey.c_str(),NULL,MITHENZIP_SHELLEXT_NAME)) {
    return false;
  }
  if(!WriteString(root,(ctxKey + L"\\InprocServer32").c_str(),NULL,modulePath)) {
    return false;
  }
  if(!WriteString(root,(ctxKey + L"\\InprocServer32").c_str(),L"ThreadingModel",L"Apartment")) {
    return false;
  }
  for(size_t i = 0; i < sizeof(kShellExPrefixes) / sizeof(kShellExPrefixes[0]); i++) {
    const std::wstring key = std::wstring(kClassesPrefix) +
        kShellExPrefixes[i] + L"\\shellex\\ContextMenuHandlers\\MithenZip";
    if(!WriteString(root,key.c_str(),NULL,MITHENZIP_CTX_CLSID_STR)) {
      return false;
    }
  }
  WriteApproved(MITHENZIP_CTX_CLSID_STR,MITHENZIP_SHELLEXT_NAME);

  //Remove the ProgID used by earlier versions.
  ::SHDeleteKeyW(root,(std::wstring(kClassesPrefix) + MITHENZIP_LEGACY_PROGID).c_str());

  const DWORD rootTag = (root == HKEY_LOCAL_MACHINE) ? 1 : 2;
  WriteDword(root,kSoftwareKey,kRootValue,rootTag);

  //Archive file extensions
  size_t extensionCount = 0;
  const wchar_t * const * extensions = MithenZip_ArchiveExtensions(extensionCount);
  for(size_t i = 0; associate && i < extensionCount; i++) {
    const std::wstring extKey = std::wstring(kClassesPrefix) + extensions[i];
    std::wstring backup;
    if(!ReadString(root,kBackupSubKey,extensions[i],backup)) {
      std::wstring previous;
      if(ReadString(root,extKey.c_str(),NULL,previous)) {
        WriteString(root,kBackupSubKey,extensions[i],previous.c_str());
      }
    }
    if(!WriteString(root,(std::wstring(kClassesPrefix) + L"Applications\\MithenZip.exe\\SupportedTypes\\" +
        extensions[i]).c_str(),NULL,L"")) {
      return false;
    }
    if(!WriteString(root,extKey.c_str(),NULL,MITHENZIP_PROGID)) {
      return false;
    }
    //Remove the SystemFileAssociations override written by earlier builds; it made
    //the Shell bind our folder in contexts that crashed Explorer.
    {
      const std::wstring staleKey = std::wstring(kClassesPrefix) +
          L"SystemFileAssociations\\" + extensions[i] + L"\\CLSID";
      std::wstring current;
      if(ReadString(root,staleKey.c_str(),NULL,current) && current == MITHENZIP_CLSID_STR) {
        const std::wstring sfaBackup = std::wstring(L"SfaCLSID") + extensions[i];
        std::wstring previous;
        if(ReadString(root,kBackupSubKey,sfaBackup.c_str(),previous) &&
            !previous.empty() && previous != MITHENZIP_CLSID_STR) {
          WriteString(root,staleKey.c_str(),NULL,previous.c_str());
        }
        else {
          ::SHDeleteKeyW(root,staleKey.c_str());
        }
      }
    }


    //Make MithenZip available in the "Open with" list.
    {
      HKEY openWith = NULL;
      const std::wstring openWithKey = extKey + L"\\OpenWithProgids";
      if(::RegCreateKeyExW(root,openWithKey.c_str(),0,NULL,REG_OPTION_NON_VOLATILE,
          KEY_WRITE,NULL,&openWith,NULL) == ERROR_SUCCESS) {
        ::RegSetValueExW(openWith,MITHENZIP_PROGID,0,REG_NONE,NULL,0);
        ::RegCloseKey(openWith);
      }
    }
  }

  ::SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
  return true;
}

//Make MithenZip the default handler for an extension using the sanctioned API.
//Windows protects a user default choice with a hash, so writing the registry
//directly is ignored/undone; SetAppAsDefault lets Windows write a valid choice.

//Windows' own archive context menus ("Extract All...") live in these two shell
//extension handlers. MithenZip installs its own Archive menu, so they are hidden
//through the per-user Blocked list and restored when MithenZip is uninstalled.
static const wchar_t * const kBlockedKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Blocked";
static const wchar_t * const kSystemMenuHandlers[] = {
  L"{EE07CEF5-3441-4CFB-870A-4002C724783A}",  // Compressed Archive Folder Menu
  L"{b8cdcb65-b1bf-4b42-9428-1dfdb7ee92af}"   // Compressed (zipped) Folder Menu
};

static bool BlockedValueExists(LPCWSTR name)
{
  HKEY key = NULL;
  bool exists = false;
  if(::RegOpenKeyExW(HKEY_CURRENT_USER,kBlockedKey,0,KEY_READ,&key) == ERROR_SUCCESS) {
    wchar_t buffer[8];
    DWORD size = sizeof(buffer);
    exists = (::RegQueryValueExW(key,name,NULL,NULL,(BYTE*)buffer,&size) == ERROR_SUCCESS);
    ::RegCloseKey(key);
  }
  return exists;
}

static void DeleteBlockedValue(LPCWSTR name)
{
  HKEY key = NULL;
  if(::RegOpenKeyExW(HKEY_CURRENT_USER,kBlockedKey,0,KEY_SET_VALUE,&key) == ERROR_SUCCESS) {
    ::RegDeleteValueW(key,name);
    ::RegCloseKey(key);
  }
}

static void BlockSystemArchiveMenus()
{
  for(size_t i = 0; i < sizeof(kSystemMenuHandlers) / sizeof(kSystemMenuHandlers[0]); i++) {
    if(BlockedValueExists(kSystemMenuHandlers[i])) {
      //Already hidden, possibly by the user; leave that choice alone.
      continue;
    }
    if(WriteString(HKEY_CURRENT_USER,kBlockedKey,kSystemMenuHandlers[i],L"")) {
      WriteString(HKEY_CURRENT_USER,kBackupSubKey,
          (std::wstring(L"Blocked") + kSystemMenuHandlers[i]).c_str(),L"1");
    }
  }
}

static void UnblockSystemArchiveMenus()
{
  for(size_t i = 0; i < sizeof(kSystemMenuHandlers) / sizeof(kSystemMenuHandlers[0]); i++) {
    std::wstring marker;
    if(ReadString(HKEY_CURRENT_USER,kBackupSubKey,
        (std::wstring(L"Blocked") + kSystemMenuHandlers[i]).c_str(),marker) && marker == L"1") {
      DeleteBlockedValue(kSystemMenuHandlers[i]);
    }
  }
}

static bool AssociationEnabled()
{
  wchar_t value[8];
  return ::GetEnvironmentVariableW(L"MITHENZIP_NO_ASSOC",value,8) == 0;
}

//Register machine-wide when possible, and always per-user, so a pre-existing
//per-user association cannot shadow ours.
static bool RegisterServer()
{
  const bool associate = AssociationEnabled();
  const HKEY primary = ResolveClassesRoot();
  if(!RegisterServerInRoot(primary,associate)) {
    return false;
  }
  if(primary == HKEY_LOCAL_MACHINE) {
    RegisterServerInRoot(HKEY_CURRENT_USER,associate);
  }

  if(associate) {
    //Windows claims archive types for its own handler and only honours an explicit
    //user choice, so record one through the sanctioned API. This needs the
    //application registration above to be effective.
    const HRESULT comResult = ::CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    IApplicationAssociationRegistration *registration = NULL;
    if(::CoCreateInstance(CLSID_ApplicationAssociationRegistration,NULL,CLSCTX_INPROC_SERVER,
        IID_IApplicationAssociationRegistration,(void**)&registration) == S_OK && registration) {
      size_t extensionCount = 0;
      const wchar_t * const * extensions = MithenZip_ArchiveExtensions(extensionCount);
      for(size_t i = 0; i < extensionCount; i++) {
        registration->SetAppAsDefault(MITHENZIP_PROGID,extensions[i],AT_FILEEXTENSION);
      }
      registration->Release();
    }
    if(SUCCEEDED(comResult)) {
      ::CoUninitialize();
    }
  }
  BlockSystemArchiveMenus();
  ::SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
  return true;
}

static void RestoreExtension(HKEY root,LPCWSTR extension)
{
  const std::wstring extKey = std::wstring(kClassesPrefix) + extension;
  std::wstring current;
  if(!ReadString(root,extKey.c_str(),NULL,current)) {
    return;
  }
  if(current != MITHENZIP_PROGID && current != MITHENZIP_LEGACY_PROGID) {
    return;
  }
  std::wstring backup;
  if(ReadString(root,kBackupSubKey,extension,backup) && !backup.empty()) {
    WriteString(root,extKey.c_str(),NULL,backup.c_str());
  }
  else {
    DeleteDefaultValue(root,extKey.c_str());
  }
  const std::wstring sfaKey = std::wstring(kClassesPrefix) +
      L"SystemFileAssociations\\" + extension + L"\\CLSID";
  const std::wstring sfaBackup = std::wstring(L"SfaCLSID") + extension;
  std::wstring sfaCurrent;
  //Only undo our own override; never touch another handler's registration.
  if(ReadString(root,sfaKey.c_str(),NULL,sfaCurrent) && sfaCurrent == MITHENZIP_CLSID_STR) {
    if(ReadString(root,kBackupSubKey,sfaBackup.c_str(),backup) && !backup.empty()) {
      WriteString(root,sfaKey.c_str(),NULL,backup.c_str());
    }
    else {
      ::SHDeleteKeyW(root,sfaKey.c_str());
    }
  }
}

static void DeleteClassesKeys(HKEY root)
{
  ::SHDeleteKeyW(root,(std::wstring(kClassesPrefix) + L"CLSID\\" + MITHENZIP_CLSID_STR).c_str());
  ::SHDeleteKeyW(root,(std::wstring(kClassesPrefix) + L"CLSID\\" + MITHENZIP_CTX_CLSID_STR).c_str());
  ::SHDeleteKeyW(root,(std::wstring(kClassesPrefix) + MITHENZIP_PROGID).c_str());
  ::SHDeleteKeyW(root,(std::wstring(kClassesPrefix) + L"Applications\\MithenZip.exe").c_str());
  ::SHDeleteKeyW(root,(std::wstring(kClassesPrefix) + MITHENZIP_LEGACY_PROGID).c_str());
  for(size_t i = 0; i < sizeof(kShellExPrefixes) / sizeof(kShellExPrefixes[0]); i++) {
    const std::wstring key = std::wstring(kClassesPrefix) +
        kShellExPrefixes[i] + L"\\shellex\\ContextMenuHandlers\\MithenZip";
    ::SHDeleteKeyW(root,key.c_str());
  }
  size_t extensionCount = 0;
  const wchar_t * const * extensions = MithenZip_ArchiveExtensions(extensionCount);
  for(size_t i = 0; i < extensionCount; i++) {
    RestoreExtension(root,extensions[i]);
    const std::wstring openWithKey = std::wstring(kClassesPrefix) + extensions[i] + L"\\OpenWithProgids";
    HKEY openWith = NULL;
    if(::RegOpenKeyExW(root,openWithKey.c_str(),0,KEY_SET_VALUE,&openWith) == ERROR_SUCCESS) {
      ::RegDeleteValueW(openWith,MITHENZIP_PROGID);
      ::RegCloseKey(openWith);
    }
  }
  ::SHDeleteKeyW(root,kSoftwareKey);
}

static bool UnregisterServer()
{
  //Restore the system menus before the bookkeeping that remembers we hid them.
  UnblockSystemArchiveMenus();
  //A registration may exist per-user and/or machine-wide; clean both.
  DeleteClassesKeys(HKEY_LOCAL_MACHINE);
  DeleteClassesKeys(HKEY_CURRENT_USER);
  DeleteApproved(MITHENZIP_CTX_CLSID_STR);
  ::SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
  return true;
}

STDAPI DllRegisterServer()
{
  return RegisterServer() ? S_OK : SELFREG_E_CLASS;
}

STDAPI DllUnregisterServer()
{
  return UnregisterServer() ? S_OK : SELFREG_E_CLASS;
}
