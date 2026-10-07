// MithenZipLauncher.cpp
// Trampoline installed as the "open" command for archive files:
//   "<install dir>\MithenZip.exe" "%1"
// It forwards to MithenZip.dll, which decides between native browsing and our
// own extraction window.

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <shellapi.h>
#include <string>

typedef HRESULT (__cdecl *MithenZipOpenArchiveFunc)(LPCWSTR archivePath);

int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int)
{
  int argc = 0;
  LPWSTR *argv = ::CommandLineToArgvW(::GetCommandLineW(),&argc);
  if(!argv) {
    return 1;
  }
  std::wstring path;
  if(argc > 1 && argv[1]) {
    path = argv[1];
  }
  ::LocalFree(argv);
  if(path.empty()) {
    return 1;
  }

  wchar_t modulePath[MAX_PATH];
  modulePath[0] = 0;
  ::GetModuleFileNameW(NULL,modulePath,MAX_PATH);
  std::wstring dllPath = modulePath;
  const size_t slash = dllPath.find_last_of(L"\\/");
  if(slash == std::wstring::npos) {
    return 1;
  }
  dllPath.erase(slash + 1);
  dllPath += L"MithenZip.dll";

  HMODULE lib = ::LoadLibraryW(dllPath.c_str());
  if(!lib) {
    return 1;
  }
  MithenZipOpenArchiveFunc openArchive =
      (MithenZipOpenArchiveFunc)(void*)::GetProcAddress(lib,"MithenZip_OpenArchive");
  if(!openArchive) {
    ::FreeLibrary(lib);
    return 1;
  }
  const HRESULT result = openArchive(path.c_str());
  ::FreeLibrary(lib);
  return SUCCEEDED(result) ? 0 : 1;
}
