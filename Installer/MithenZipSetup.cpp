// MithenZipSetup.cpp
// Native installer for the MithenZip shell namespace extension.
// Shows a simple dialog (associate file types / restart Explorer), with a
// progress bar; embeds MithenZip.dll + 7z.dll.
// Supports /uninstall and /s (silent).

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <string>

#define IDR_PAYLOAD_DLL 101
#define IDR_PAYLOAD_7Z  102
#define IDR_PAYLOAD_EXE 103
#define IDR_PAYLOAD_EXE 103

#define IDD_MITHENZIP_SETUP 200
#define IDC_SETUP_TEXT      1001
#define IDX_SETUP_ASSOC     1002
#define IDX_SETUP_RESTART   1003
#define IDC_SETUP_PROGRESS  1004

#define kInstallSteps 6

static const wchar_t *kAppName   = L"MithenZip";
static const wchar_t *kVersion   = L"1.0.0";
static const wchar_t *kPublisher = L"MithenZip";
static const wchar_t *kArpSubKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MithenZip";

static std::wstring g_exePath;
static bool g_usedInUseFile = false;
static bool g_needsReboot = false;

struct CSetupOptions
{
  bool associate;
  bool restartExplorer;
  CSetupOptions(): associate(true),restartExplorer(true) {}
};

static bool IsElevated()
{
  HANDLE token = NULL;
  bool elevated = false;
  if(::OpenProcessToken(::GetCurrentProcess(),TOKEN_QUERY,&token)) {
    TOKEN_ELEVATION info;
    DWORD size = sizeof(info);
    if(::GetTokenInformation(token,TokenElevation,&info,size,&size)) {
      elevated = (info.TokenIsElevated != 0);
    }
    ::CloseHandle(token);
  }
  return elevated;
}

static std::wstring FormatError(DWORD code)
{
  wchar_t *buffer = NULL;
  const DWORD length = ::FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,code,0,(LPWSTR)&buffer,0,NULL);
  std::wstring text;
  if(length && buffer) {
    text.assign(buffer,length);
    while(!text.empty()) {
      const wchar_t last = text[text.size() - 1];
      if(last == L'\r' || last == L'\n' || last == L' ' || last == L'.') {
        text.erase(text.size() - 1);
      }
      else {
        break;
      }
    }
  }
  ::LocalFree(buffer);
  if(text.empty()) {
    text = L"error";
  }
  return text;
}

static bool GetFolderPath(int csidl,std::wstring &out)
{
  wchar_t path[MAX_PATH];
  if(::SHGetFolderPathW(NULL,csidl,NULL,SHGFP_TYPE_CURRENT,path) == S_OK) {
    out = path;
    return true;
  }
  return false;
}

static std::wstring GetInstallDir(bool elevated)
{
  std::wstring base;
  const bool ok = elevated ? GetFolderPath(CSIDL_PROGRAM_FILES,base)
                           : GetFolderPath(CSIDL_LOCAL_APPDATA,base);
  if(!ok || base.empty()) {
    return std::wstring();
  }
  if(base[base.size() - 1] == L'\\') {
    base.erase(base.size() - 1);
  }
  return base + L"\\MithenZip";
}

//Write an embedded resource to "path". If the target is in use (for example the
//shell extension is loaded in explorer.exe) it is renamed aside and the old copy
//is scheduled for deletion at the next reboot.
static bool WriteResourceToFile(HINSTANCE instance,int resId,const std::wstring &path,std::wstring &error)
{
  HRSRC resource = ::FindResourceW(instance,MAKEINTRESOURCEW(resId),RT_RCDATA);
  if(!resource) {
    error = L"embedded resource is missing";
    return false;
  }
  const DWORD size = ::SizeofResource(instance,resource);
  HGLOBAL data = ::LoadResource(instance,resource);
  const void *ptr = data ? ::LockResource(data) : NULL;
  if(!ptr) {
    error = L"embedded resource could not be read";
    return false;
  }
  const std::wstring tempPath = path + L".new";
  ::DeleteFileW(tempPath.c_str());
  HANDLE file = ::CreateFileW(tempPath.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file == INVALID_HANDLE_VALUE) {
    error = FormatError(::GetLastError()) + L" (" + tempPath + L")";
    return false;
  }
  DWORD written = 0;
  bool ok = ::WriteFile(file,ptr,size,&written,NULL) && written == size;
  ::CloseHandle(file);
  if(!ok) {
    error = FormatError(::GetLastError()) + L" (" + tempPath + L")";
    ::DeleteFileW(tempPath.c_str());
    return false;
  }

  if(::MoveFileExW(tempPath.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)) {
    return true;
  }
  //The target is in use; loaded image files can still be renamed.
  const std::wstring oldPath = path + L".old";
  ::DeleteFileW(oldPath.c_str());
  if(::MoveFileExW(path.c_str(),oldPath.c_str(),MOVEFILE_REPLACE_EXISTING)) {
    if(::MoveFileExW(tempPath.c_str(),path.c_str(),0)) {
      ::MoveFileExW(oldPath.c_str(),NULL,MOVEFILE_DELAY_UNTIL_REBOOT);
      g_usedInUseFile = true;
      return true;
    }
    ::MoveFileExW(oldPath.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);
  }
  if(::MoveFileExW(tempPath.c_str(),path.c_str(),
      MOVEFILE_REPLACE_EXISTING | MOVEFILE_DELAY_UNTIL_REBOOT)) {
    g_needsReboot = true;
    return true;
  }
  ::DeleteFileW(tempPath.c_str());
  error = FormatError(::GetLastError()) + L" (" + path + L")";
  return false;
}

static bool CallDllExport(const std::wstring &dllPath,const char *procName)
{
  HMODULE lib = ::LoadLibraryW(dllPath.c_str());
  if(!lib) {
    return false;
  }
  typedef HRESULT (STDAPICALLTYPE *Func)(void);
  Func fn = (Func)(void*)::GetProcAddress(lib,procName);
  bool ok = false;
  if(fn) {
    ok = SUCCEEDED(fn());
  }
  ::FreeLibrary(lib);
  return ok;
}

static void SetRegistryString(HKEY root,LPCWSTR subKey,LPCWSTR name,LPCWSTR value)
{
  HKEY key = NULL;
  if(::RegCreateKeyExW(root,subKey,0,NULL,REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&key,NULL) == ERROR_SUCCESS) {
    ::RegSetValueExW(key,name,0,REG_SZ,(const BYTE*)value,(DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
    ::RegCloseKey(key);
  }
}

static bool ReadRegistryString(HKEY root,LPCWSTR subKey,LPCWSTR name,std::wstring &value)
{
  HKEY key = NULL;
  if(::RegOpenKeyExW(root,subKey,0,KEY_READ,&key) != ERROR_SUCCESS) {
    return false;
  }
  wchar_t buffer[MAX_PATH * 2];
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

static void SetProgress(HWND hwnd,int position)
{
  if(!hwnd) {
    return;
  }
  ::SendMessageW(::GetDlgItem(hwnd,IDC_SETUP_PROGRESS),PBM_SETPOS,(WPARAM)position,0);
  MSG message;
  while(::PeekMessageW(&message,NULL,0,0,PM_REMOVE)) {
    if(!::IsDialogMessageW(hwnd,&message)) {
      ::TranslateMessage(&message);
      ::DispatchMessageW(&message);
    }
  }
}

static void RestartExplorer()
{
  wchar_t command[] = L"cmd.exe /c taskkill /f /im explorer.exe >nul 2>&1 & start \"\" explorer.exe";
  STARTUPINFOW si;
  memset(&si,0,sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;
  memset(&pi,0,sizeof(pi));
  if(::CreateProcessW(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)) {
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
  }
}

static bool Install(HINSTANCE instance,const CSetupOptions &options,bool silent,
    HWND progressHwnd,std::wstring &error)
{
  int step = 0;
  const bool elevated = IsElevated();
  const std::wstring dir = GetInstallDir(elevated);
  if(dir.empty()) {
    error = L"Could not determine the installation folder.";
    return false;
  }
  if(!::CreateDirectoryW(dir.c_str(),NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS) {
    error = L"Could not create:\n" + dir + L"\n\n" + FormatError(::GetLastError());
    return false;
  }
  SetProgress(progressHwnd,++step);

  const std::wstring dllPath = dir + L"\\MithenZip.dll";
  if(!WriteResourceToFile(instance,IDR_PAYLOAD_DLL,dllPath,error)) {
    error = L"Could not write MithenZip.dll.\n\n" + error;
    return false;
  }
  SetProgress(progressHwnd,++step);

  if(!WriteResourceToFile(instance,IDR_PAYLOAD_7Z,dir + L"\\7z.dll",error)) {
    error = L"Could not write 7z.dll.\n\n" + error;
    return false;
  }
  if(!WriteResourceToFile(instance,IDR_PAYLOAD_EXE,dir + L"\\MithenZip.exe",error)) {
    error = L"Could not write MithenZip.exe.\n\n" + error;
    return false;
  }
  SetProgress(progressHwnd,++step);

  //Let DllRegisterServer know whether to associate file types.
  ::SetEnvironmentVariableW(L"MITHENZIP_NO_ASSOC",options.associate ? NULL : L"1");
  const bool registered = CallDllExport(dllPath,"DllRegisterServer");
  ::SetEnvironmentVariableW(L"MITHENZIP_NO_ASSOC",NULL);
  if(!registered) {
    error = L"Could not register the MithenZip shell extension.";
    return false;
  }
  SetProgress(progressHwnd,++step);

  const std::wstring uninstallExe = dir + L"\\Uninstall.exe";
  ::CopyFileW(g_exePath.c_str(),uninstallExe.c_str(),FALSE);

  const HKEY root = elevated ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  SetRegistryString(root,kArpSubKey,L"DisplayName",kAppName);
  SetRegistryString(root,kArpSubKey,L"DisplayVersion",kVersion);
  SetRegistryString(root,kArpSubKey,L"Publisher",kPublisher);
  SetRegistryString(root,kArpSubKey,L"InstallLocation",dir.c_str());
  SetRegistryString(root,kArpSubKey,L"DisplayIcon",uninstallExe.c_str());
  const std::wstring uninstallCmd = std::wstring(L"\"") + uninstallExe + L"\" /uninstall";
  SetRegistryString(root,kArpSubKey,L"UninstallString",uninstallCmd.c_str());
  SetRegistryString(root,kArpSubKey,L"QuietUninstallString",(uninstallCmd + L" /s").c_str());

  HKEY key = NULL;
  if(::RegCreateKeyExW(root,kArpSubKey,0,NULL,REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&key,NULL) == ERROR_SUCCESS) {
    const DWORD one = 1;
    ::RegSetValueExW(key,L"NoModify",0,REG_DWORD,(const BYTE*)&one,sizeof(one));
    ::RegCloseKey(key);
  }
  SetProgress(progressHwnd,++step);

  if(options.restartExplorer) {
    RestartExplorer();
  }
  SetProgress(progressHwnd,++step);

  (void)g_usedInUseFile;
  return true;
}

//Delete a file, or move it aside and let Windows remove it at the next restart
//when it is still in use (a loaded DLL, or the running uninstaller).
static bool DeleteOrSchedule(const std::wstring &path,bool &needsReboot)
{
  if(::DeleteFileW(path.c_str())) {
    return true;
  }
  const std::wstring oldPath = path + L".old";
  ::DeleteFileW(oldPath.c_str());
  if(::MoveFileExW(path.c_str(),oldPath.c_str(),MOVEFILE_REPLACE_EXISTING)) {
    ::MoveFileExW(oldPath.c_str(),NULL,MOVEFILE_DELAY_UNTIL_REBOOT);
    needsReboot = true;
    return false;
  }
  ::MoveFileExW(path.c_str(),NULL,MOVEFILE_DELAY_UNTIL_REBOOT);
  needsReboot = true;
  return false;
}

static bool Uninstall(bool silent)
{
  std::wstring dir;
  HKEY arpRoot = NULL;
  if(ReadRegistryString(HKEY_LOCAL_MACHINE,kArpSubKey,L"InstallLocation",dir)) {
    arpRoot = HKEY_LOCAL_MACHINE;
  }
  else if(ReadRegistryString(HKEY_CURRENT_USER,kArpSubKey,L"InstallLocation",dir)) {
    arpRoot = HKEY_CURRENT_USER;
  }
  if(dir.empty()) {
    std::wstring base;
    if(GetFolderPath(CSIDL_LOCAL_APPDATA,base)) {
      dir = base + L"\\MithenZip";
    }
  }

  const std::wstring dllPath = dir + L"\\MithenZip.dll";
  CallDllExport(dllPath,"DllUnregisterServer");
  if(arpRoot) {
    ::SHDeleteKeyW(arpRoot,kArpSubKey);
  }
  ::SHDeleteKeyW(HKEY_LOCAL_MACHINE,kArpSubKey);
  ::SHDeleteKeyW(HKEY_CURRENT_USER,kArpSubKey);

  bool needsReboot = false;
  const std::wstring pattern = dir + L"\\*";
  WIN32_FIND_DATAW entry;
  memset(&entry,0,sizeof(entry));
  HANDLE find = ::FindFirstFileW(pattern.c_str(),&entry);
  if(find != INVALID_HANDLE_VALUE) {
    do {
      if(wcscmp(entry.cFileName,L".") == 0 || wcscmp(entry.cFileName,L"..") == 0) {
        continue;
      }
      DeleteOrSchedule(dir + L"\\" + entry.cFileName,needsReboot);
    } while(::FindNextFileW(find,&entry));
    ::FindClose(find);
  }
  ::SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
  if(!::RemoveDirectoryW(dir.c_str())) {
    ::MoveFileExW(dir.c_str(),NULL,MOVEFILE_DELAY_UNTIL_REBOOT);
    needsReboot = true;
  }

  (void)needsReboot;
  return true;
}

struct CDialogState
{
  bool finished;
  CDialogState(): finished(false) {}
};

static INT_PTR CALLBACK SetupDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
  CDialogState *state = (CDialogState*)::GetWindowLongPtrW(hwnd,DWLP_USER);
  switch(message) {
    case WM_INITDIALOG:
      state = new CDialogState();
      ::SetWindowLongPtrW(hwnd,DWLP_USER,(LONG_PTR)state);
      //Associate only on a first install; never disturb an existing setup.
      {
      bool priorInstall = false;
      {
        HKEY probe = NULL;
        if(::RegOpenKeyExW(HKEY_LOCAL_MACHINE,kArpSubKey,0,KEY_READ,&probe) == ERROR_SUCCESS) {
          priorInstall = true;
          ::RegCloseKey(probe);
        }
        else if(::RegOpenKeyExW(HKEY_CURRENT_USER,kArpSubKey,0,KEY_READ,&probe) == ERROR_SUCCESS) {
          priorInstall = true;
          ::RegCloseKey(probe);
        }
        else {
          const std::wstring installed = GetInstallDir(IsElevated()) + L"\\MithenZip.dll";
          priorInstall = (::GetFileAttributesW(installed.c_str()) != INVALID_FILE_ATTRIBUTES);
        }
      }
      ::CheckDlgButton(hwnd,IDX_SETUP_ASSOC,priorInstall ? BST_UNCHECKED : BST_CHECKED);
      ::CheckDlgButton(hwnd,IDX_SETUP_RESTART,BST_UNCHECKED);
      }
      ::ShowWindow(::GetDlgItem(hwnd,IDC_SETUP_PROGRESS),SW_HIDE);
      ::SendMessageW(::GetDlgItem(hwnd,IDC_SETUP_PROGRESS),PBM_SETRANGE32,0,kInstallSteps);
      {
        HICON icon = (HICON)::LoadImageW(::GetModuleHandleW(NULL),MAKEINTRESOURCEW(1),IMAGE_ICON,0,0,
            LR_DEFAULTSIZE | LR_SHARED);
        if(icon) {
          ::SendMessageW(hwnd,WM_SETICON,ICON_BIG,(LPARAM)icon);
          ::SendMessageW(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)icon);
        }
      }
      return TRUE;
    case WM_COMMAND:
      switch(LOWORD(wParam)) {
        case IDOK:
          if(state && state->finished) {
            ::EndDialog(hwnd,TRUE);
            return TRUE;
          }
          {
            CSetupOptions options;
            options.associate = (::IsDlgButtonChecked(hwnd,IDX_SETUP_ASSOC) == BST_CHECKED);
            options.restartExplorer = (::IsDlgButtonChecked(hwnd,IDX_SETUP_RESTART) == BST_CHECKED);
            ::EnableWindow(::GetDlgItem(hwnd,IDX_SETUP_ASSOC),FALSE);
            ::EnableWindow(::GetDlgItem(hwnd,IDX_SETUP_RESTART),FALSE);
            ::EnableWindow(::GetDlgItem(hwnd,IDOK),FALSE);
            ::EnableWindow(::GetDlgItem(hwnd,IDCANCEL),FALSE);
            ::ShowWindow(::GetDlgItem(hwnd,IDC_SETUP_PROGRESS),SW_SHOW);
            SetProgress(hwnd,0);

            std::wstring error;
            const bool ok = Install((HINSTANCE)::GetModuleHandleW(NULL),options,false,hwnd,error);
            if(ok) {
              ::SetDlgItemTextW(hwnd,IDC_SETUP_TEXT,L"MithenZip installed successfully.");
              ::ShowWindow(::GetDlgItem(hwnd,IDX_SETUP_ASSOC),SW_HIDE);
              ::ShowWindow(::GetDlgItem(hwnd,IDX_SETUP_RESTART),SW_HIDE);
              ::ShowWindow(::GetDlgItem(hwnd,IDC_SETUP_PROGRESS),SW_HIDE);
              ::SetDlgItemTextW(hwnd,IDOK,L"Finish");
              ::EnableWindow(::GetDlgItem(hwnd,IDOK),TRUE);
              ::SetFocus(::GetDlgItem(hwnd,IDOK));
              ::ShowWindow(::GetDlgItem(hwnd,IDCANCEL),SW_HIDE);
              if(state) {
                state->finished = true;
              }
            }
            else {
              ::MessageBoxW(hwnd,error.c_str(),kAppName,MB_OK | MB_ICONERROR);
              ::EnableWindow(::GetDlgItem(hwnd,IDX_SETUP_ASSOC),TRUE);
              ::EnableWindow(::GetDlgItem(hwnd,IDX_SETUP_RESTART),TRUE);
              ::EnableWindow(::GetDlgItem(hwnd,IDOK),TRUE);
              ::EnableWindow(::GetDlgItem(hwnd,IDCANCEL),TRUE);
              ::ShowWindow(::GetDlgItem(hwnd,IDC_SETUP_PROGRESS),SW_HIDE);
            }
          }
          return TRUE;
        case IDCANCEL:
          ::EndDialog(hwnd,FALSE);
          return TRUE;
      }
      return FALSE;
    case WM_CLOSE:
      if(state && state->finished) {
        ::EndDialog(hwnd,TRUE);
      }
      return TRUE;
    case WM_DESTROY:
      if(state) {
        ::SetWindowLongPtrW(hwnd,DWLP_USER,0);
        delete state;
      }
      return TRUE;
  }
  return FALSE;
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE /*prevInstance*/,LPWSTR /*commandLine*/,int /*show*/)
{
  wchar_t buffer[MAX_PATH];
  buffer[0] = 0;
  ::GetModuleFileNameW(NULL,buffer,MAX_PATH);
  g_exePath = buffer;

  INITCOMMONCONTROLSEX controls;
  controls.dwSize = sizeof(controls);
  controls.dwICC = ICC_PROGRESS_CLASS;
  ::InitCommonControlsEx(&controls);

  std::wstring command = ::GetCommandLineW();
  for(size_t i = 0; i < command.size(); i++) {
    if(command[i] >= L'A' && command[i] <= L'Z') {
      command[i] = (wchar_t)(command[i] - L'A' + L'a');
    }
  }
  const bool silent = command.find(L"/s") != std::wstring::npos;
  bool doUninstall = command.find(L"/uninstall") != std::wstring::npos;

  //A copy named Uninstall.exe uninstalls when double-clicked.
  std::wstring exeName = g_exePath;
  const size_t slash = exeName.find_last_of(L"\\/");
  if(slash != std::wstring::npos) {
    exeName.erase(0,slash + 1);
  }
  if(_wcsnicmp(exeName.c_str(),L"Uninstall",9) == 0) {
    doUninstall = true;
  }

  if(doUninstall) {
    return Uninstall(silent) ? 0 : 1;
  }
  if(silent) {
    CSetupOptions options;
    std::wstring error;
    if(!Install(instance,options,true,NULL,error)) {
      return 1;
    }
    return 0;
  }

  ::DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_MITHENZIP_SETUP),NULL,SetupDialogProc,0);
  return 0;
}
