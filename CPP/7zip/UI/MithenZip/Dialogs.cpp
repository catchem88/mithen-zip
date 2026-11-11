// Dialogs.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "MithenZip.h"
#include "ArchiveEngine.h"
#include "ArchiveOps.h"
#include "DialogsRes.h"
#include "Dialogs.h"

static void SetDialogIcon(HWND hwnd);

#define WM_MITHENZIP_PROGRESS (WM_APP + 1)

//////////////////////////////////////////////////////////////////////////
// control helpers

static void ComboAdd(HWND hwnd,int id,const wchar_t *text,LPARAM data)
{
  HWND combo = ::GetDlgItem(hwnd,id);
  const LRESULT index = ::SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)text);
  if(index >= 0) {
    ::SendMessageW(combo,CB_SETITEMDATA,(WPARAM)index,(LPARAM)data);
  }
}

static void ComboSelectByData(HWND hwnd,int id,LPARAM data)
{
  HWND combo = ::GetDlgItem(hwnd,id);
  const LRESULT count = ::SendMessageW(combo,CB_GETCOUNT,0,0);
  for(LRESULT i = 0; i < count; i++) {
    if(::SendMessageW(combo,CB_GETITEMDATA,(WPARAM)i,0) == data) {
      ::SendMessageW(combo,CB_SETCURSEL,(WPARAM)i,0);
      return;
    }
  }
  ::SendMessageW(combo,CB_SETCURSEL,0,0);
}

static LPARAM ComboGetData(HWND hwnd,int id)
{
  HWND combo = ::GetDlgItem(hwnd,id);
  const LRESULT sel = ::SendMessageW(combo,CB_GETCURSEL,0,0);
  if(sel < 0) {
    return 0;
  }
  return ::SendMessageW(combo,CB_GETITEMDATA,(WPARAM)sel,0);
}

static std::wstring GetDlgText(HWND hwnd,int id)
{
  const int length = ::GetWindowTextLengthW(::GetDlgItem(hwnd,id));
  std::wstring text;
  text.resize((size_t)length + 1);
  ::GetWindowTextW(::GetDlgItem(hwnd,id),&text[0],length + 1);
  text.resize((size_t)length);
  return text;
}

static bool IsNoUi()
{
  wchar_t value[8];
  return ::GetEnvironmentVariableW(L"MITHENZIP_NO_UI",value,8) != 0;
}

static std::wstring GetEnvPassword()
{
  wchar_t value[512];
  if(::GetEnvironmentVariableW(L"MITHENZIP_PASSWORD",value,512) != 0) {
    return std::wstring(value);
  }
  return std::wstring();
}

static void GetWritableFormats(std::vector<std::wstring> &formats)
{
  formats.clear();
  CMithenZipFormatRegistry &registry = CMithenZipFormatRegistry::Instance();
  if(registry.Ready()) {
    registry.GetWritableFormatNames(formats);
  }
}

static const wchar_t * FormatExtension(const std::wstring &format)
{
  if(_wcsicmp(format.c_str(),L"zip") == 0) {
    return L".zip";
  }
  if(_wcsicmp(format.c_str(),L"7z") == 0) {
    return L".7z";
  }
  if(_wcsicmp(format.c_str(),L"tar") == 0) {
    return L".tar";
  }
  if(_wcsicmp(format.c_str(),L"gzip") == 0) {
    return L".gz";
  }
  if(_wcsicmp(format.c_str(),L"bzip2") == 0) {
    return L".bz2";
  }
  if(_wcsicmp(format.c_str(),L"xz") == 0) {
    return L".xz";
  }
  if(_wcsicmp(format.c_str(),L"wim") == 0) {
    return L".wim";
  }
  return L"";
}

//Replace the file name extension of a path (leaving the directory alone).
static std::wstring ReplaceExtension(const std::wstring &path,const std::wstring &extension)
{
  if(extension.empty()) {
    return path;
  }
  const size_t sep = path.find_last_of(L"\\/");
  const size_t dot = path.find_last_of(L'.');
  if(dot != std::wstring::npos && (sep == std::wstring::npos || dot > sep)) {
    return path.substr(0,dot) + extension;
  }
  return path + extension;
}

//////////////////////////////////////////////////////////////////////////
// option tables

static const struct { int Level; const wchar_t *Name; } kLevels[] = {
  {0,L"0 - Store"},{1,L"1 - Fastest"},{2,L"2"},{3,L"3 - Fast"},{4,L"4"},
  {5,L"5 - Normal"},{6,L"6"},{7,L"7 - Maximum"},{8,L"8"},{9,L"9 - Ultra"}
};

static const wchar_t * const kMethods[] = {
  L"Copy",L"LZMA",L"LZMA2",L"PPMd",L"BZip2",L"Deflate",L"Deflate64",L"GNU",L"POSIX"
};

static const struct { UInt64 Bytes; const wchar_t *Name; } kDictionary[] = {
  {0,L"* Auto"},{1 << 16,L"64 KB"},{1 << 18,L"256 KB"},{1 << 20,L"1 MB"},{1 << 22,L"4 MB"},
  {1 << 24,L"16 MB"},{1 << 26,L"64 MB"},{(UInt64)128 << 20,L"128 MB"},{(UInt64)256 << 20,L"256 MB"},
  {(UInt64)512 << 20,L"512 MB"},{(UInt64)1 << 30,L"1 GB"},{(UInt64)1536 << 20,L"1.5 GB"}
};

static const struct { UInt32 Value; const wchar_t *Name; } kWordSize[] = {
  {0,L"* Auto"},{8,L"8"},{16,L"16"},{24,L"24"},{32,L"32"},{48,L"48"},
  {64,L"64"},{96,L"96"},{128,L"128"},{192,L"192"},{273,L"273"}
};

static const struct { UInt64 Bytes; const wchar_t *Name; } kSolid[] = {
  {0,L"* Auto"},{1 << 24,L"16 MB"},{1 << 25,L"32 MB"},{1 << 26,L"64 MB"},
  {(UInt64)128 << 20,L"128 MB"},{(UInt64)256 << 20,L"256 MB"},{(UInt64)512 << 20,L"512 MB"},
  {(UInt64)1 << 30,L"1 GB"},{(UInt64)2 << 30,L"2 GB"},{(UInt64)4 << 30,L"4 GB"}
};

static const struct { UInt32 Value; const wchar_t *Name; } kThreads[] = {
  {0,L"* Auto"},{1,L"1"},{2,L"2"},{3,L"3"},{4,L"4"},{6,L"6"},{8,L"8"},
  {12,L"12"},{16,L"16"},{24,L"24"},{32,L"32"},{48,L"48"},{64,L"64"}
};

static const wchar_t * const kUpdateModes[] = {
  L"Add and replace files",L"Update and add files",
  L"Freshen existing files",L"Synchronize files"
};

static const wchar_t * const kPathModesAdd[] = {
  L"Relative paths",L"Full paths",L"Absolute paths"
};

static const wchar_t * const kEncMethods[] = { L"AES-256" };

static const int kExtractPathModes[] = {
  kMithenZipPathExtract_Full,kMithenZipPathExtract_No,kMithenZipPathExtract_Absolute
};

static const wchar_t * const kExtractPathModeNames[] = {
  L"Full paths",L"No paths",L"Absolute paths"
};

static const wchar_t * const kOverwriteNames[] = {
  L"Ask before overwrite",L"Overwrite without prompt",L"Skip existing files",
  L"Auto rename",L"Auto rename existing file"
};

//////////////////////////////////////////////////////////////////////////
// Add to Archive dialog

struct CAddDialogParams
{
  std::vector<std::wstring> inputs;
  CMithenZipCompressOptions *options;
  std::wstring *archivePath;
};

static void SyncArchiveExtension(HWND hwnd)
{
  std::vector<std::wstring> formats;
  GetWritableFormats(formats);
  const LRESULT index = ::SendMessageW(::GetDlgItem(hwnd,IDC_ADD_FORMAT),CB_GETCURSEL,0,0);
  if(index < 0 || (size_t)index >= formats.size()) {
    return;
  }
  const wchar_t *extension = FormatExtension(formats[(size_t)index]);
  if(!extension[0]) {
    return;
  }
  const std::wstring current = GetDlgText(hwnd,IDE_ADD_ARCHIVE);
  if(current.empty()) {
    return;
  }
  ::SetDlgItemTextW(hwnd,IDE_ADD_ARCHIVE,ReplaceExtension(current,extension).c_str());
}

static void FillAddDialog(HWND hwnd,CAddDialogParams *params)
{
  CMithenZipCompressOptions &options = *params->options;

  ::SetDlgItemTextW(hwnd,IDE_ADD_ARCHIVE,params->archivePath->c_str());

  std::vector<std::wstring> formats;
  GetWritableFormats(formats);
  for(size_t i = 0; i < formats.size(); i++) {
    ComboAdd(hwnd,IDC_ADD_FORMAT,formats[i].c_str(),(LPARAM)(INT_PTR)i);
  }
  {
    int select = 0;
    for(size_t i = 0; i < formats.size(); i++) {
      if(_wcsicmp(formats[i].c_str(),options.FormatName.c_str()) == 0) {
        select = (int)i;
        break;
      }
    }
    ::SendMessageW(::GetDlgItem(hwnd,IDC_ADD_FORMAT),CB_SETCURSEL,(WPARAM)select,0);
  }

  for(size_t i = 0; i < sizeof(kLevels) / sizeof(kLevels[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_LEVEL,kLevels[i].Name,(LPARAM)kLevels[i].Level);
  }
  ComboSelectByData(hwnd,IDC_ADD_LEVEL,options.Level);

  ComboAdd(hwnd,IDC_ADD_METHOD,L"* Auto",(LPARAM)-1);
  for(size_t i = 0; i < sizeof(kMethods) / sizeof(kMethods[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_METHOD,kMethods[i],(LPARAM)(INT_PTR)i);
  }
  {
    LPARAM methodData = -1;
    if(!options.Method.empty()) {
      for(size_t i = 0; i < sizeof(kMethods) / sizeof(kMethods[0]); i++) {
        if(_wcsicmp(kMethods[i],options.Method.c_str()) == 0) {
          methodData = (LPARAM)(INT_PTR)i;
          break;
        }
      }
    }
    ComboSelectByData(hwnd,IDC_ADD_METHOD,methodData);
  }

  for(size_t i = 0; i < sizeof(kDictionary) / sizeof(kDictionary[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_DICTIONARY,kDictionary[i].Name,(LPARAM)(INT_PTR)kDictionary[i].Bytes);
  }
  ComboSelectByData(hwnd,IDC_ADD_DICTIONARY,(LPARAM)(INT_PTR)options.Dictionary);

  for(size_t i = 0; i < sizeof(kWordSize) / sizeof(kWordSize[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_WORDSIZE,kWordSize[i].Name,(LPARAM)kWordSize[i].Value);
  }
  ComboSelectByData(hwnd,IDC_ADD_WORDSIZE,(LPARAM)options.WordSize);

  for(size_t i = 0; i < sizeof(kSolid) / sizeof(kSolid[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_SOLID,kSolid[i].Name,(LPARAM)(INT_PTR)kSolid[i].Bytes);
  }
  ComboSelectByData(hwnd,IDC_ADD_SOLID,(LPARAM)(INT_PTR)options.SolidBlock);

  for(size_t i = 0; i < sizeof(kThreads) / sizeof(kThreads[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_THREADS,kThreads[i].Name,(LPARAM)kThreads[i].Value);
  }
  ComboSelectByData(hwnd,IDC_ADD_THREADS,(LPARAM)options.NumThreads);

  for(size_t i = 0; i < sizeof(kUpdateModes) / sizeof(kUpdateModes[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_UPDATEMODE,kUpdateModes[i],(LPARAM)(INT_PTR)i);
  }
  ComboSelectByData(hwnd,IDC_ADD_UPDATEMODE,(LPARAM)options.UpdateMode);

  for(size_t i = 0; i < sizeof(kPathModesAdd) / sizeof(kPathModesAdd[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_PATHMODE,kPathModesAdd[i],(LPARAM)(INT_PTR)i);
  }
  ComboSelectByData(hwnd,IDC_ADD_PATHMODE,(LPARAM)options.PathMode);

  ::CheckDlgButton(hwnd,IDX_ADD_ENCFILES,options.EncryptFileNames ? BST_CHECKED : BST_UNCHECKED);
  ::SetDlgItemTextW(hwnd,IDE_ADD_PASSWORD,options.Password.c_str());
  ::SetDlgItemTextW(hwnd,IDE_ADD_PARAMS,options.ExtraParameters.c_str());

  for(size_t i = 0; i < sizeof(kEncMethods) / sizeof(kEncMethods[0]); i++) {
    ComboAdd(hwnd,IDC_ADD_ENCMETHOD,kEncMethods[i],(LPARAM)(INT_PTR)i);
  }
  ::SendMessageW(::GetDlgItem(hwnd,IDC_ADD_ENCMETHOD),CB_SETCURSEL,0,0);
}

static void ReadAddDialog(HWND hwnd,CAddDialogParams *params)
{
  CMithenZipCompressOptions &options = *params->options;
  *params->archivePath = GetDlgText(hwnd,IDE_ADD_ARCHIVE);

  std::vector<std::wstring> formats;
  GetWritableFormats(formats);
  const LRESULT formatIndex = ::SendMessageW(::GetDlgItem(hwnd,IDC_ADD_FORMAT),CB_GETCURSEL,0,0);
  if(formatIndex >= 0 && (size_t)formatIndex < formats.size()) {
    options.FormatName = formats[(size_t)formatIndex];
  }

  options.Level = (int)ComboGetData(hwnd,IDC_ADD_LEVEL);
  {
    const LPARAM methodData = ComboGetData(hwnd,IDC_ADD_METHOD);
    if(methodData >= 0 && (size_t)methodData < sizeof(kMethods) / sizeof(kMethods[0])) {
      options.Method = kMethods[methodData];
    }
    else {
      options.Method.clear();
    }
  }
  options.Dictionary = (UInt64)ComboGetData(hwnd,IDC_ADD_DICTIONARY);
  options.WordSize = (UInt32)ComboGetData(hwnd,IDC_ADD_WORDSIZE);
  options.SolidBlock = (UInt64)ComboGetData(hwnd,IDC_ADD_SOLID);
  options.NumThreads = (int)ComboGetData(hwnd,IDC_ADD_THREADS);
  options.UpdateMode = (int)ComboGetData(hwnd,IDC_ADD_UPDATEMODE);
  options.PathMode = (int)ComboGetData(hwnd,IDC_ADD_PATHMODE);
  options.EncryptFileNames = (::IsDlgButtonChecked(hwnd,IDX_ADD_ENCFILES) == BST_CHECKED);
  options.Password = GetDlgText(hwnd,IDE_ADD_PASSWORD);
  options.ExtraParameters = GetDlgText(hwnd,IDE_ADD_PARAMS);

  const LRESULT encIndex = ::SendMessageW(::GetDlgItem(hwnd,IDC_ADD_ENCMETHOD),CB_GETCURSEL,0,0);
  options.EncryptionMethod = (encIndex == 0) ? L"AES256" : std::wstring();
}

static INT_PTR CALLBACK AddDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
  CAddDialogParams *params = (CAddDialogParams*)::GetWindowLongPtrW(hwnd,DWLP_USER);
  switch(message) {
    case WM_INITDIALOG:
      params = (CAddDialogParams*)lParam;
      ::SetWindowLongPtrW(hwnd,DWLP_USER,(LONG_PTR)params);
      FillAddDialog(hwnd,params);
      SetDialogIcon(hwnd);
      return TRUE;
    case WM_COMMAND:
      switch(LOWORD(wParam)) {
        case IDC_ADD_FORMAT:
          if(HIWORD(wParam) == CBN_SELCHANGE) {
            SyncArchiveExtension(hwnd);
            return TRUE;
          }
          return FALSE;
        case IDB_ADD_BROWSE:
        {
          wchar_t fileName[MAX_PATH];
          fileName[0] = 0;
          const std::wstring current = GetDlgText(hwnd,IDE_ADD_ARCHIVE);
          wcsncpy_s(fileName,MAX_PATH,current.c_str(),_TRUNCATE);
          OPENFILENAMEW ofn;
          memset(&ofn,0,sizeof(ofn));
          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = hwnd;
          ofn.lpstrFilter = L"All files\0*.*\0";
          ofn.lpstrFile = fileName;
          ofn.nMaxFile = MAX_PATH;
          ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
          if(::GetSaveFileNameW(&ofn)) {
            ::SetDlgItemTextW(hwnd,IDE_ADD_ARCHIVE,fileName);
          }
          return TRUE;
        }
        case IDX_ADD_SHOWPSW:
        {
          const bool show = (::IsDlgButtonChecked(hwnd,IDX_ADD_SHOWPSW) == BST_CHECKED);
          ::SendMessageW(::GetDlgItem(hwnd,IDE_ADD_PASSWORD),EM_SETPASSWORDCHAR,show ? 0 : 0x25CF,0);
          ::SendMessageW(::GetDlgItem(hwnd,IDE_ADD_PASSWORD2),EM_SETPASSWORDCHAR,show ? 0 : 0x25CF,0);
          ::InvalidateRect(::GetDlgItem(hwnd,IDE_ADD_PASSWORD),NULL,TRUE);
          ::InvalidateRect(::GetDlgItem(hwnd,IDE_ADD_PASSWORD2),NULL,TRUE);
          return TRUE;
        }
        case IDOK:
        {
          const std::wstring password1 = GetDlgText(hwnd,IDE_ADD_PASSWORD);
          const std::wstring password2 = GetDlgText(hwnd,IDE_ADD_PASSWORD2);
          if(password1 != password2) {
            ::MessageBoxW(hwnd,L"The passwords do not match. Please reenter them.",
                L"MithenZip",MB_OK | MB_ICONWARNING);
            ::SetFocus(::GetDlgItem(hwnd,IDE_ADD_PASSWORD2));
            return TRUE;
          }
          if(params) {
            ReadAddDialog(hwnd,params);
          }
          ::EndDialog(hwnd,TRUE);
          return TRUE;
        }
        case IDCANCEL:
          ::EndDialog(hwnd,FALSE);
          return TRUE;
      }
      return FALSE;
  }
  return FALSE;
}

bool MithenZip_ShowAddDialog(HWND owner,const std::vector<std::wstring> &inputs,
    std::wstring &archivePath,CMithenZipCompressOptions &options)
{
  const std::wstring envPassword = GetEnvPassword();
  if(!envPassword.empty() && options.Password.empty()) {
    options.Password = envPassword;
  }
  if(IsNoUi()) {
    wchar_t format[32];
    if(::GetEnvironmentVariableW(L"MITHENZIP_FORMAT",format,32) != 0) {
      std::vector<std::wstring> formats;
      GetWritableFormats(formats);
      for(size_t i = 0; i < formats.size(); i++) {
        if(_wcsicmp(formats[i].c_str(),format) == 0) {
          options.FormatName = formats[i];
          archivePath = ReplaceExtension(archivePath,FormatExtension(formats[i]));
          break;
        }
      }
    }
    return true;
  }
  CAddDialogParams params;
  params.inputs = inputs;
  params.options = &options;
  params.archivePath = &archivePath;
  const INT_PTR result = ::DialogBoxParamW(g_hInstance,MAKEINTRESOURCEW(IDD_MITHENZIP_ADD),
      owner,AddDialogProc,(LPARAM)&params);
  return result == TRUE;
}

//////////////////////////////////////////////////////////////////////////
// Extract dialog

struct CExtractDialogParams
{
  CMithenZipExtractOptions *options;
};

static void FillExtractDialog(HWND hwnd,CExtractDialogParams *params)
{
  CMithenZipExtractOptions &options = *params->options;
  ::SetDlgItemTextW(hwnd,IDE_EXTRACT_PATH,options.OutputDir.c_str());

  for(size_t i = 0; i < sizeof(kExtractPathModes) / sizeof(kExtractPathModes[0]); i++) {
    ComboAdd(hwnd,IDC_EXTRACT_PATHMODE,kExtractPathModeNames[i],(LPARAM)(INT_PTR)kExtractPathModes[i]);
  }
  ComboSelectByData(hwnd,IDC_EXTRACT_PATHMODE,(LPARAM)options.PathMode);

  for(size_t i = 0; i < sizeof(kOverwriteNames) / sizeof(kOverwriteNames[0]); i++) {
    ComboAdd(hwnd,IDC_EXTRACT_OVERWRITE,kOverwriteNames[i],(LPARAM)(INT_PTR)i);
  }
  ComboSelectByData(hwnd,IDC_EXTRACT_OVERWRITE,(LPARAM)options.OverwriteMode);

  ::CheckDlgButton(hwnd,IDX_EXTRACT_ELIMDUP,options.ElimDup ? BST_CHECKED : BST_UNCHECKED);
  ::SetDlgItemTextW(hwnd,IDE_EXTRACT_PASSWORD,options.Password.c_str());
}

static void ReadExtractDialog(HWND hwnd,CExtractDialogParams *params)
{
  CMithenZipExtractOptions &options = *params->options;
  options.OutputDir = GetDlgText(hwnd,IDE_EXTRACT_PATH);
  options.PathMode = (int)ComboGetData(hwnd,IDC_EXTRACT_PATHMODE);
  options.OverwriteMode = (int)ComboGetData(hwnd,IDC_EXTRACT_OVERWRITE);
  options.ElimDup = (::IsDlgButtonChecked(hwnd,IDX_EXTRACT_ELIMDUP) == BST_CHECKED);
  options.Password = GetDlgText(hwnd,IDE_EXTRACT_PASSWORD);
}

static INT_PTR CALLBACK ExtractDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
  CExtractDialogParams *params = (CExtractDialogParams*)::GetWindowLongPtrW(hwnd,DWLP_USER);
  switch(message) {
    case WM_INITDIALOG:
      params = (CExtractDialogParams*)lParam;
      ::SetWindowLongPtrW(hwnd,DWLP_USER,(LONG_PTR)params);
      FillExtractDialog(hwnd,params);
      SetDialogIcon(hwnd);
      //Show the password by default so it can be checked while typing.
      ::CheckDlgButton(hwnd,IDX_EXTRACT_SHOWPSW,BST_CHECKED);
      ::SendMessageW(::GetDlgItem(hwnd,IDE_EXTRACT_PASSWORD),EM_SETPASSWORDCHAR,0,0);
      return TRUE;
    case WM_COMMAND:
      switch(LOWORD(wParam)) {
        case IDB_EXTRACT_BROWSE:
        {
          BROWSEINFOW browse;
          memset(&browse,0,sizeof(browse));
          browse.hwndOwner = hwnd;
          browse.lpszTitle = L"Select the folder to extract to:";
          browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
          LPITEMIDLIST pidl = ::SHBrowseForFolderW(&browse);
          if(pidl) {
            wchar_t path[MAX_PATH];
            path[0] = 0;
            if(::SHGetPathFromIDListW(pidl,path)) {
              ::SetDlgItemTextW(hwnd,IDE_EXTRACT_PATH,path);
            }
            CoTaskMemFree(pidl);
          }
          return TRUE;
        }
        case IDX_EXTRACT_SHOWPSW:
        {
          const bool show = (::IsDlgButtonChecked(hwnd,IDX_EXTRACT_SHOWPSW) == BST_CHECKED);
          ::SendMessageW(::GetDlgItem(hwnd,IDE_EXTRACT_PASSWORD),EM_SETPASSWORDCHAR,show ? 0 : 0x25CF,0);
          ::InvalidateRect(::GetDlgItem(hwnd,IDE_EXTRACT_PASSWORD),NULL,TRUE);
          return TRUE;
        }
        case IDOK:
          if(params) {
            ReadExtractDialog(hwnd,params);
          }
          ::EndDialog(hwnd,TRUE);
          return TRUE;
        case IDCANCEL:
          ::EndDialog(hwnd,FALSE);
          return TRUE;
      }
      return FALSE;
  }
  return FALSE;
}

bool MithenZip_ShowExtractDialog(HWND owner,CMithenZipExtractOptions &options)
{
  if(IsNoUi()) {
    return true;
  }
  CExtractDialogParams params;
  params.options = &options;
  const INT_PTR result = ::DialogBoxParamW(g_hInstance,MAKEINTRESOURCEW(IDD_MITHENZIP_EXTRACT),
      owner,ExtractDialogProc,(LPARAM)&params);
  return result == TRUE;
}

//////////////////////////////////////////////////////////////////////////
// progress dialog

CMithenZipProgressDialog::CMithenZipProgressDialog():
    _hwnd(NULL),_ownerThread(0),_total(0),_completed(0),_lastPost(0),_cancelled(0)
{
  ::InitializeCriticalSection(&_cs);
}

CMithenZipProgressDialog::~CMithenZipProgressDialog()
{
  Destroy();
  ::DeleteCriticalSection(&_cs);
}

void CMithenZipProgressDialog::UpdateBar()
{
  if(!_hwnd || !::IsWindow(_hwnd)) {
    return;
  }
  UInt64 total = 0;
  UInt64 completed = 0;
  std::wstring text;
  ::EnterCriticalSection(&_cs);
  total = _total;
  completed = _completed;
  text = _text;
  ::LeaveCriticalSection(&_cs);
  LONG position = 0;
  if(total > 0) {
    const UInt64 scaled = (completed * 1000) / total;
    position = (LONG)((scaled > 1000) ? 1000 : scaled);
  }
  ::SendMessageW(::GetDlgItem(_hwnd,IDC_PROGRESS_BAR),PBM_SETPOS,(WPARAM)position,0);
  if(!text.empty()) {
    ::SetDlgItemTextW(_hwnd,IDC_PROGRESS_TEXT,text.c_str());
  }
}

bool CMithenZipProgressDialog::Create(HWND owner,const std::wstring &title)
{
  //Allow headless runs (tests) to suppress the progress window.
  if(IsNoUi()) {
    return false;
  }
  _ownerThread = ::GetCurrentThreadId();
  _hwnd = ::CreateDialogParamW(g_hInstance,MAKEINTRESOURCEW(IDD_MITHENZIP_PROGRESS),
      owner,DialogProc,(LPARAM)this);
  if(!_hwnd) {
    return false;
  }
  ::SendMessageW(::GetDlgItem(_hwnd,IDC_PROGRESS_BAR),PBM_SETRANGE32,0,1000);
  ::SetWindowTextW(_hwnd,title.c_str());
  //Refresh on a timer too: some formats report progress rarely, and the bar must
  //not look frozen while the encoder is busy.
  ::SetTimer(_hwnd,1,200,NULL);
  if(owner) {
    RECT ownerRect;
    RECT dialogRect;
    if(::GetWindowRect(owner,&ownerRect) && ::GetWindowRect(_hwnd,&dialogRect)) {
      const int width = dialogRect.right - dialogRect.left;
      const int height = dialogRect.bottom - dialogRect.top;
      const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
      const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
      ::SetWindowPos(_hwnd,NULL,x,y,0,0,SWP_NOSIZE | SWP_NOZORDER);
    }
  }
  ::ShowWindow(_hwnd,SW_SHOW);
  PumpMessages();
  return true;
}

void CMithenZipProgressDialog::Destroy()
{
  if(_hwnd) {
    if(::GetCurrentThreadId() == _ownerThread && ::IsWindow(_hwnd)) {
      ::DestroyWindow(_hwnd);
    }
    _hwnd = NULL;
  }
}

void CMithenZipProgressDialog::PumpMessages()
{
  if(!_hwnd || ::GetCurrentThreadId() != _ownerThread || !::IsWindow(_hwnd)) {
    return;
  }
  MSG message;
  while(::PeekMessageW(&message,NULL,0,0,PM_REMOVE)) {
    if(!::IsDialogMessageW(_hwnd,&message)) {
      ::TranslateMessage(&message);
      ::DispatchMessageW(&message);
    }
  }
}

void CMithenZipProgressDialog::RunUntilDone(HANDLE doneEvent)
{
  if(doneEvent) {
    for(;;) {
      const DWORD result = ::MsgWaitForMultipleObjects(1,&doneEvent,FALSE,INFINITE,QS_ALLINPUT);
      if(result == WAIT_OBJECT_0) {
        break;
      }
      if(result == WAIT_OBJECT_0 + 1) {
        PumpMessages();
        continue;
      }
      break;
    }
  }
  PumpMessages();
}

void CMithenZipProgressDialog::ProgressSetTotal(UInt64 total)
{
  ::EnterCriticalSection(&_cs);
  _total = total;
  ::LeaveCriticalSection(&_cs);
  if(_hwnd) {
    ::PostMessageW(_hwnd,WM_MITHENZIP_PROGRESS,0,0);
  }
}

void CMithenZipProgressDialog::ProgressSetCompleted(UInt64 completed)
{
  bool post = false;
  ::EnterCriticalSection(&_cs);
  _completed = completed;
  if(_hwnd && ::GetCurrentThreadId() != _ownerThread) {
    //Encoder threads report very often; throttle the cross-thread notifications.
    const DWORD now = ::GetTickCount();
    if((DWORD)(now - _lastPost) >= 50) {
      _lastPost = now;
      post = true;
    }
  }
  ::LeaveCriticalSection(&_cs);
  if(!_hwnd) {
    return;
  }
  if(::GetCurrentThreadId() == _ownerThread) {
    //UI work and message pumping only on the owning thread.
    UpdateBar();
    PumpMessages();
  }
  else if(post) {
    ::PostMessageW(_hwnd,WM_MITHENZIP_PROGRESS,0,0);
  }
}

void CMithenZipProgressDialog::ProgressSetText(const std::wstring &text)
{
  ::EnterCriticalSection(&_cs);
  _text = text;
  ::LeaveCriticalSection(&_cs);
  if(!_hwnd) {
    return;
  }
  if(::GetCurrentThreadId() == _ownerThread) {
    UpdateBar();
  }
  else {
    ::PostMessageW(_hwnd,WM_MITHENZIP_PROGRESS,0,0);
  }
}

bool CMithenZipProgressDialog::ProgressIsCancelled()
{
  PumpMessages();
  return _cancelled != 0;
}

void CMithenZipProgressDialog::ProgressPump()
{
  PumpMessages();
}

HWND CMithenZipProgressDialog::ProgressOwner()
{
  return _hwnd;
}

INT_PTR CALLBACK CMithenZipProgressDialog::DialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
  CMithenZipProgressDialog *self = (CMithenZipProgressDialog*)::GetWindowLongPtrW(hwnd,DWLP_USER);
  switch(message) {
    case WM_INITDIALOG:
      self = (CMithenZipProgressDialog*)lParam;
      ::SetWindowLongPtrW(hwnd,DWLP_USER,(LONG_PTR)self);
      SetDialogIcon(hwnd);
      return TRUE;
    case WM_MITHENZIP_PROGRESS:
      if(self) {
        self->UpdateBar();
      }
      return TRUE;
    case WM_TIMER:
      if(self && wParam == 1) {
        self->UpdateBar();
      }
      return TRUE;
    case WM_COMMAND:
      if(LOWORD(wParam) == IDCANCEL) {
        if(self) {
          ::InterlockedExchange(&self->_cancelled,1);
        }
        ::EnableWindow(::GetDlgItem(hwnd,IDCANCEL),FALSE);
        return TRUE;
      }
      return FALSE;
    case WM_CLOSE:
      if(self) {
        ::InterlockedExchange(&self->_cancelled,1);
      }
      return TRUE;
  }
  return FALSE;
}

//////////////////////////////////////////////////////////////////////////
// password prompt

struct CPasswordDialogParams
{
  std::wstring *password;
};

static INT_PTR CALLBACK PasswordDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
  CPasswordDialogParams *params = (CPasswordDialogParams*)::GetWindowLongPtrW(hwnd,DWLP_USER);
  switch(message) {
    case WM_INITDIALOG:
      params = (CPasswordDialogParams*)lParam;
      ::SetWindowLongPtrW(hwnd,DWLP_USER,(LONG_PTR)params);
      SetDialogIcon(hwnd);
      return TRUE;
    case WM_COMMAND:
      switch(LOWORD(wParam)) {
        case IDX_PSW_SHOW:
        {
          const bool show = (::IsDlgButtonChecked(hwnd,IDX_PSW_SHOW) == BST_CHECKED);
          ::SendMessageW(::GetDlgItem(hwnd,IDE_PSW_EDIT),EM_SETPASSWORDCHAR,show ? 0 : 0x25CF,0);
          ::InvalidateRect(::GetDlgItem(hwnd,IDE_PSW_EDIT),NULL,TRUE);
          return TRUE;
        }
        case IDOK:
          if(params) {
            *params->password = GetDlgText(hwnd,IDE_PSW_EDIT);
          }
          ::EndDialog(hwnd,TRUE);
          return TRUE;
        case IDCANCEL:
          ::EndDialog(hwnd,FALSE);
          return TRUE;
      }
      return FALSE;
  }
  return FALSE;
}

bool MithenZip_ShowPasswordDialog(HWND owner,std::wstring &password)
{
  const std::wstring envPassword = GetEnvPassword();
  if(!envPassword.empty()) {
    password = envPassword;
    return true;
  }
  if(IsNoUi()) {
    return false;
  }
  CPasswordDialogParams params;
  params.password = &password;
  const INT_PTR result = ::DialogBoxParamW(g_hInstance,MAKEINTRESOURCEW(IDD_MITHENZIP_PASSWORD),
      owner,PasswordDialogProc,(LPARAM)&params);
  return result == TRUE;
}

//////////////////////////////////////////////////////////////////////////
// overwrite prompt

struct COverwriteDialogParams
{
  std::wstring *path;
  CMithenZipOverwriteAnswer answer;
};

static INT_PTR CALLBACK OverwriteDialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam)
{
  COverwriteDialogParams *params = (COverwriteDialogParams*)::GetWindowLongPtrW(hwnd,DWLP_USER);
  switch(message) {
    case WM_INITDIALOG:
      params = (COverwriteDialogParams*)lParam;
      ::SetWindowLongPtrW(hwnd,DWLP_USER,(LONG_PTR)params);
      SetDialogIcon(hwnd);
      if(params && params->path) {
        ::SetDlgItemTextW(hwnd,IDC_OVERWRITE_TEXT,
            (L"The file already exists:\n\n" + *params->path +
             L"\n\nDo you want to replace it?").c_str());
      }
      //Enter must default to Replace.
      ::SetFocus(::GetDlgItem(hwnd,IDB_OVERWRITE_REPLACE));
      return FALSE;
    case WM_COMMAND:
      switch(LOWORD(wParam)) {
        case IDOK:
        case IDB_OVERWRITE_REPLACE:
          if(params) {
            params->answer = kMithenZipOverwriteAnswer_Replace;
          }
          ::EndDialog(hwnd,TRUE);
          return TRUE;
        case IDB_OVERWRITE_REPLACEALL:
          if(params) {
            params->answer = kMithenZipOverwriteAnswer_ReplaceAll;
          }
          ::EndDialog(hwnd,TRUE);
          return TRUE;
        case IDB_OVERWRITE_KEEPBOTH:
          if(params) {
            params->answer = kMithenZipOverwriteAnswer_KeepBoth;
          }
          ::EndDialog(hwnd,TRUE);
          return TRUE;
        case IDB_OVERWRITE_SKIP:
          if(params) {
            params->answer = kMithenZipOverwriteAnswer_Skip;
          }
          ::EndDialog(hwnd,TRUE);
          return TRUE;
        case IDCANCEL:
          if(params) {
            params->answer = kMithenZipOverwriteAnswer_Cancel;
          }
          ::EndDialog(hwnd,FALSE);
          return TRUE;
      }
      return FALSE;
    case WM_CLOSE:
      if(params) {
        params->answer = kMithenZipOverwriteAnswer_Cancel;
      }
      ::EndDialog(hwnd,FALSE);
      return TRUE;
  }
  return FALSE;
}

CMithenZipOverwriteAnswer MithenZip_ShowOverwriteDialog(HWND owner,const std::wstring &path)
{
  //Headless runs must not block on a prompt; replace like a batch tool.
  if(IsNoUi()) {
    return kMithenZipOverwriteAnswer_Replace;
  }
  COverwriteDialogParams params;
  params.path = (std::wstring*)&path;
  params.answer = kMithenZipOverwriteAnswer_Cancel;
  ::DialogBoxParamW(g_hInstance,MAKEINTRESOURCEW(IDD_MITHENZIP_OVERWRITE),
      owner,OverwriteDialogProc,(LPARAM)&params);
  return params.answer;
}


HWND MithenZip_PromptOwner()
{
  HWND owner = ::GetActiveWindow();
  if(!owner) {
    owner = ::GetForegroundWindow();
  }
  return owner;
}

//Put the MithenZip logo in the dialog's title bar.
static void SetDialogIcon(HWND hwnd)
{
  //The icon lives in MithenZip.dll, not in the host process (explorer.exe).
  const int largeSize = ::GetSystemMetrics(SM_CXICON);
  const int smallSize = ::GetSystemMetrics(SM_CXSMICON);
  HICON largeIcon = (HICON)::LoadImageW(g_hInstance,MAKEINTRESOURCEW(1),
      IMAGE_ICON,largeSize,largeSize,LR_SHARED);
  HICON smallIcon = (HICON)::LoadImageW(g_hInstance,MAKEINTRESOURCEW(1),
      IMAGE_ICON,smallSize,smallSize,LR_SHARED);
  if(!largeIcon) {
    largeIcon = ::LoadIconW(NULL,IDI_APPLICATION);
  }
  if(!smallIcon) {
    smallIcon = ::LoadIconW(NULL,IDI_APPLICATION);
  }
  if(largeIcon) {
    ::SendMessageW(hwnd,WM_SETICON,ICON_BIG,(LPARAM)largeIcon);
  }
  if(smallIcon) {
    ::SendMessageW(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)smallIcon);
  }
}

//////////////////////////////////////////////////////////////////////////
// background operation runner

struct COperationContext
{
  HWND Owner;
  int Kind;
  std::wstring ArchivePath;
  std::vector<std::wstring> Inputs;
  CMithenZipCompressOptions Compress;
  CMithenZipExtractOptions Extract;
  CMithenZipProgress *Progress;
  HANDLE Done;
  HRESULT Result;
  HRESULT *ResultPtr;

  COperationContext():
      Owner(NULL),Kind(kMithenZipOp_Compress),Progress(NULL),Done(NULL),
      Result(E_FAIL),ResultPtr(NULL)
    {}
};

static const wchar_t * OperationTitle(int kind)
{
  return (kind == kMithenZipOp_Compress)
      ? L"Adding to archive - MithenZip"
      : L"Extracting - MithenZip";
}

static void ReportOperationResult(HWND owner,HRESULT result)
{
  if(result == S_OK || result == E_ABORT) {
    return;
  }
  if(IsNoUi()) {
    return;
  }
  if(!::IsWindow(owner)) {
    owner = NULL;
  }
  if(result == MITHENZIP_E_WRONG_PASSWORD) {
    ::MessageBoxW(owner,L"The password is incorrect. Extraction stopped.",L"MithenZip",MB_OK | MB_ICONERROR);
    return;
  }
  wchar_t buffer[256];
  swprintf_s(buffer,256,L"The operation failed (0x%08lX).",(unsigned long)result);
  ::MessageBoxW(owner,buffer,L"MithenZip",MB_OK | MB_ICONERROR);
}

static HRESULT RunOperationOnCallingThread(COperationContext &context)
{
  CMithenZipProgressDialog progress;
  progress.Create(context.Owner,OperationTitle(context.Kind));
  context.Progress = &progress;
  HRESULT result = E_FAIL;
  if(context.Kind == kMithenZipOp_Compress) {
    result = MithenZip_Compress(&progress,context.Inputs,context.ArchivePath,context.Compress);
  }
  else {
    result = MithenZip_Extract(&progress,context.ArchivePath,context.Extract);
  }
  progress.Destroy();
  ReportOperationResult(context.Owner,result);
  return result;
}

static DWORD WINAPI OperationWorkerThread(LPVOID param)
{
  COperationContext *context = (COperationContext*)param;
  if(context->Kind == kMithenZipOp_Compress) {
    context->Result = MithenZip_Compress(context->Progress,context->Inputs,
        context->ArchivePath,context->Compress);
  }
  else {
    context->Result = MithenZip_Extract(context->Progress,context->ArchivePath,
        context->Extract);
  }
  ::SetEvent(context->Done);
  return 0;
}

//Owns the progress window. Runs its own message loop so the progress callbacks
//(which arrive from the encoder threads) are painted while the archive work runs.
static DWORD WINAPI OperationDialogThread(LPVOID param)
{
  COperationContext *context = (COperationContext*)param;
  const HWND owner = context->Owner;
  const HANDLE done = context->Done;

  CMithenZipProgressDialog progress;
  progress.Create(owner,OperationTitle(context->Kind));
  context->Progress = &progress;

  const HANDLE worker = ::CreateThread(NULL,0,OperationWorkerThread,context,0,NULL);
  if(worker) {
    progress.RunUntilDone(done);
    ::WaitForSingleObject(worker,INFINITE);
    ::CloseHandle(worker);
  }
  else {
    context->Result = E_FAIL;
  }

  const HRESULT result = context->Result;
  progress.Destroy();
  if(context->ResultPtr) {
    *context->ResultPtr = result;
  }
  ::SetEvent(done);
  ::CloseHandle(done);
  delete context;
  ReportOperationResult(owner,result);
  ::InterlockedDecrement(&g_dllRefCount);
  return 0;
}

static void FillOperationContext(COperationContext &context,HWND owner,int kind,
    const std::wstring &archivePath,const std::vector<std::wstring> &inputs,
    const CMithenZipCompressOptions *compress,const CMithenZipExtractOptions *extract)
{
  context.Owner = owner;
  context.Kind = kind;
  context.ArchivePath = archivePath;
  context.Inputs = inputs;
  if(compress) {
    context.Compress = *compress;
  }
  if(extract) {
    context.Extract = *extract;
  }
}

//Starts the operation and returns the dialog thread handle (NULL on failure).
static HANDLE StartOperationThread(COperationContext &source,HRESULT *resultPtr)
{
  COperationContext *context = new COperationContext(source);
  context->ResultPtr = resultPtr;
  context->Done = ::CreateEventW(NULL,TRUE,FALSE,NULL);
  if(!context->Done) {
    delete context;
    return NULL;
  }
  //Keep this DLL loaded (DllCanUnloadNow) for as long as the operation runs.
  ::InterlockedIncrement(&g_dllRefCount);
  const HANDLE thread = ::CreateThread(NULL,0,OperationDialogThread,context,0,NULL);
  if(!thread) {
    ::CloseHandle(context->Done);
    delete context;
    ::InterlockedDecrement(&g_dllRefCount);
    return NULL;
  }
  return thread;
}

bool MithenZip_StartOperation(HWND owner,int kind,const std::wstring &archivePath,
    const std::vector<std::wstring> &inputs,
    const CMithenZipCompressOptions *compress,
    const CMithenZipExtractOptions *extract)
{
  COperationContext context;
  FillOperationContext(context,owner,kind,archivePath,inputs,compress,extract);
  //Headless runs must stay synchronous so their results are deterministic.
  if(IsNoUi()) {
    RunOperationOnCallingThread(context);
    return true;
  }
  const HANDLE thread = StartOperationThread(context,NULL);
  if(!thread) {
    ReportOperationResult(owner,E_OUTOFMEMORY);
    return false;
  }
  //The operation is now owned by the dialog thread; let it run independently.
  ::CloseHandle(thread);
  return true;
}

HRESULT MithenZip_RunOperation(HWND owner,int kind,const std::wstring &archivePath,
    const std::vector<std::wstring> &inputs,
    const CMithenZipCompressOptions *compress,
    const CMithenZipExtractOptions *extract)
{
  COperationContext context;
  FillOperationContext(context,owner,kind,archivePath,inputs,compress,extract);
  //Headless runs must stay synchronous so their results are deterministic.
  if(IsNoUi()) {
    return RunOperationOnCallingThread(context);
  }
  HRESULT result = E_FAIL;
  const HANDLE thread = StartOperationThread(context,&result);
  if(!thread) {
    return E_OUTOFMEMORY;
  }
  ::WaitForSingleObject(thread,INFINITE);
  ::CloseHandle(thread);
  return result;
}
