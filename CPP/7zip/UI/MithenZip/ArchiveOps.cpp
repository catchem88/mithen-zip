// ArchiveOps.cpp

#include "StdAfx.h"

#include "../../../Common/MyCom.h"
#include "../../../Common/ComTry.h"

#include "../../IStream.h"
#include "../../Archive/IArchive.h"
#include "../../IPassword.h"

#include "MithenZip.h"
#include "ArchiveEngine.h"
#include "Streams.h"
#include "ArchiveOps.h"
#include "Dialogs.h"

//////////////////////////////////////////////////////////////////////////
// path helpers

std::wstring MithenZip_ParentDir(const std::wstring &path)
{
  const size_t sep = path.find_last_of(L"\\/");
  if(sep == std::wstring::npos) {
    return std::wstring();
  }
  return path.substr(0,sep + 1);
}

std::wstring MithenZip_FileName(const std::wstring &path)
{
  const size_t sep = path.find_last_of(L"\\/");
  if(sep == std::wstring::npos) {
    return path;
  }
  return path.substr(sep + 1);
}

std::wstring MithenZip_RemoveExtension(const std::wstring &name)
{
  const size_t dot = name.find_last_of(L'.');
  if(dot == std::wstring::npos || dot == 0) {
    return name;
  }
  return name.substr(0,dot);
}

std::wstring MithenZip_SelectionBaseName(const std::vector<std::wstring> &inputPaths)
{
  if(inputPaths.empty()) {
    return std::wstring(L"Archive");
  }
  if(inputPaths.size() == 1) {
    const std::wstring name = MithenZip_RemoveExtension(MithenZip_FileName(inputPaths[0]));
    return name.empty() ? std::wstring(L"Archive") : name;
  }
  std::wstring parent = MithenZip_ParentDir(inputPaths[0]);
  while(!parent.empty() && (parent[parent.size() - 1] == L'\\' || parent[parent.size() - 1] == L'/')) {
    parent.erase(parent.size() - 1);
  }
  const std::wstring name = MithenZip_FileName(parent);
  return name.empty() ? std::wstring(L"Archive") : name;
}

static std::wstring JoinPath(const std::wstring &dir,const std::wstring &name)
{
  if(dir.empty()) {
    return name;
  }
  const wchar_t last = dir[dir.size() - 1];
  if(last == L'\\' || last == L'/') {
    return dir + name;
  }
  return dir + L"\\" + name;
}

static bool FileExists(const std::wstring &path)
{
  return ::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool EnsureDirectory(const std::wstring &dir)
{
  if(dir.empty()) {
    return true;
  }
  const DWORD attributes = ::GetFileAttributesW(dir.c_str());
  if(attributes != INVALID_FILE_ATTRIBUTES) {
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  }
  const std::wstring parent = MithenZip_ParentDir(dir);
  if(!parent.empty() && parent.size() < dir.size()) {
    if(!EnsureDirectory(parent)) {
      return false;
    }
  }
  return ::CreateDirectoryW(dir.c_str(),NULL) != FALSE;
}

static bool DeleteTree(const std::wstring &path)
{
  const DWORD attributes = ::GetFileAttributesW(path.c_str());
  if(attributes == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    ::SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_NORMAL);
    return ::DeleteFileW(path.c_str()) != FALSE;
  }
  WIN32_FIND_DATAW findData;
  HANDLE find = ::FindFirstFileW((path + L"\\*").c_str(),&findData);
  if(find != INVALID_HANDLE_VALUE) {
    do {
      if(findData.cFileName[0] == L'.' &&
          (findData.cFileName[1] == 0 ||
           (findData.cFileName[1] == L'.' && findData.cFileName[2] == 0))) {
        continue;
      }
      DeleteTree(JoinPath(path,findData.cFileName));
    } while(::FindNextFileW(find,&findData));
    ::FindClose(find);
  }
  return ::RemoveDirectoryW(path.c_str()) != FALSE;
}

static std::wstring MakeUniqueName(const std::wstring &path)
{
  const std::wstring dir = MithenZip_ParentDir(path);
  const std::wstring name = MithenZip_FileName(path);
  const size_t dot = name.find_last_of(L'.');
  std::wstring base = name;
  std::wstring ext;
  if(dot != std::wstring::npos && dot != 0) {
    base = name.substr(0,dot);
    ext = name.substr(dot);
  }
  for(int i = 1; i < 100000; i++) {
    wchar_t suffix[16];
    swprintf_s(suffix,16,L"_%d",i);
    const std::wstring candidate = JoinPath(dir,base + suffix + ext);
    if(!FileExists(candidate)) {
      return candidate;
    }
  }
  return path;
}

//////////////////////////////////////////////////////////////////////////
// item enumeration for compression

struct COpItem
{
  std::wstring FsPath;
  std::wstring ArcPath;
  bool IsDir;
  UInt64 Size;
  UInt32 Attrib;
  FILETIME MTime;
  bool FromArchive;
  UInt32 ArchiveIndex;

  COpItem(): IsDir(false),Size(0),Attrib(0),FromArchive(false),ArchiveIndex(0)
  {
    MTime.dwLowDateTime = 0;
    MTime.dwHighDateTime = 0;
  }
};

static std::wstring ArcRootFor(const std::wstring &fsPath,int pathMode)
{
  if(pathMode == kMithenZipPathAdd_Relative) {
    return MithenZip_FileName(fsPath);
  }
  if(pathMode == kMithenZipPathAdd_Absolute) {
    std::wstring p = fsPath;
    if(p.compare(0,4,L"\\\\?\\") == 0) {
      p = p.substr(4);
    }
    return p;
  }
  std::wstring p = fsPath;
  if(p.size() >= 2 && p[1] == L':') {
    p = p.substr(2);
  }
  while(!p.empty() && (p[0] == L'\\' || p[0] == L'/')) {
    p.erase(p.begin());
  }
  return p;
}

static void AddItemRecursive(const std::wstring &fsPath,const std::wstring &arcPath,
    const std::wstring &skipPath,std::vector<COpItem> &items)
{
  if(!skipPath.empty() && _wcsicmp(fsPath.c_str(),skipPath.c_str()) == 0) {
    return;
  }
  WIN32_FILE_ATTRIBUTE_DATA info;
  if(!::GetFileAttributesExW(fsPath.c_str(),GetFileExInfoStandard,&info)) {
    return;
  }
  COpItem item;
  item.FsPath = fsPath;
  item.ArcPath = arcPath;
  item.IsDir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  item.Attrib = info.dwFileAttributes;
  item.Size = ((UInt64)info.nFileSizeHigh << 32) | info.nFileSizeLow;
  item.MTime = info.ftLastWriteTime;
  items.push_back(item);

  if(!item.IsDir) {
    return;
  }
  WIN32_FIND_DATAW findData;
  HANDLE find = ::FindFirstFileW((fsPath + L"\\*").c_str(),&findData);
  if(find == INVALID_HANDLE_VALUE) {
    return;
  }
  do {
    if(findData.cFileName[0] == L'.' &&
        (findData.cFileName[1] == 0 ||
         (findData.cFileName[1] == L'.' && findData.cFileName[2] == 0))) {
      continue;
    }
    AddItemRecursive(JoinPath(fsPath,findData.cFileName),
        arcPath + L"\\" + findData.cFileName,skipPath,items);
  } while(::FindNextFileW(find,&findData));
  ::FindClose(find);
}

static void BuildNewItems(const std::vector<std::wstring> &inputs,int pathMode,
    const std::wstring &skipPath,std::vector<COpItem> &items)
{
  for(size_t i = 0; i < inputs.size(); i++) {
    AddItemRecursive(inputs[i],ArcRootFor(inputs[i],pathMode),skipPath,items);
  }
}

//////////////////////////////////////////////////////////////////////////
// PROPVARIANT helpers

static void SetPropBstr(PROPVARIANT &p,const std::wstring &s)
{
  p.wReserved1 = p.wReserved2 = p.wReserved3 = 0;
  p.vt = VT_BSTR;
  p.bstrVal = ::SysAllocString(s.c_str());
}

static void SetPropBool(PROPVARIANT &p,bool v)
{
  p.wReserved1 = p.wReserved2 = p.wReserved3 = 0;
  p.vt = VT_BOOL;
  p.boolVal = v ? VARIANT_TRUE : VARIANT_FALSE;
}

static void SetPropUInt64(PROPVARIANT &p,UInt64 v)
{
  p.wReserved1 = p.wReserved2 = p.wReserved3 = 0;
  p.vt = VT_UI8;
  p.uhVal.QuadPart = v;
}

static void SetPropUInt32(PROPVARIANT &p,UInt32 v)
{
  p.wReserved1 = p.wReserved2 = p.wReserved3 = 0;
  p.vt = VT_UI4;
  p.ulVal = v;
}

static void SetPropFileTime(PROPVARIANT &p,const FILETIME &ft)
{
  p.wReserved1 = p.wReserved2 = p.wReserved3 = 0;
  p.vt = VT_FILETIME;
  p.filetime = ft;
}


//////////////////////////////////////////////////////////////////////////
// update callback (compression)

class CUpdateCallback Z7_final:
  public IArchiveUpdateCallback2,
  public ICryptoGetTextPassword2,
  public CMyUnknownImp
{
  Z7_IFACES_IMP_UNK_2(IArchiveUpdateCallback2,ICryptoGetTextPassword2)
  Z7_IFACE_COM7_IMP(IProgress)
  Z7_IFACE_COM7_IMP(IArchiveUpdateCallback)

  const std::vector<COpItem> *_items;
  std::wstring _password;
  bool _passwordDefined;
  CMithenZipProgress *_progress;

public:
  UInt64 NumErrors;

  CUpdateCallback(): _items(NULL),_passwordDefined(false),_progress(NULL),NumErrors(0) {}

  void Init(const std::vector<COpItem> &items,CMithenZipProgress *progress,
      const std::wstring &password,bool passwordDefined)
  {
    _items = &items;
    _progress = progress;
    _password = password;
    _passwordDefined = passwordDefined;
    NumErrors = 0;
  }
};

Z7_COM7F_IMF(CUpdateCallback::SetTotal(UInt64 total))
{
  if(_progress) {
    _progress->ProgressSetTotal(total);
  }
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback::SetCompleted(const UInt64 *completeValue))
{
  if(_progress) {
    if(completeValue) {
      _progress->ProgressSetCompleted(*completeValue);
    }
    if(_progress->ProgressIsCancelled()) {
      return E_ABORT;
    }
  }
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback::GetUpdateItemInfo(UInt32 index,
    Int32 *newData,Int32 *newProperties,UInt32 *indexInArchive))
{
  bool fromArchive = false;
  UInt32 archiveIndex = (UInt32)(Int32)-1;
  if(_items && index < _items->size()) {
    fromArchive = (*_items)[index].FromArchive;
    archiveIndex = (*_items)[index].ArchiveIndex;
  }
  if(newData) {
    *newData = fromArchive ? 0 : 1;
  }
  if(newProperties) {
    *newProperties = fromArchive ? 0 : 1;
  }
  if(indexInArchive) {
    *indexInArchive = fromArchive ? archiveIndex : (UInt32)(Int32)-1;
  }
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback::GetProperty(UInt32 index,PROPID propID,PROPVARIANT *value))
{
  if(!value) {
    return E_POINTER;
  }
  ::PropVariantInit(value);
  if(!_items || index >= _items->size()) {
    return S_OK;
  }
  const COpItem &item = (*_items)[index];
  if(item.FromArchive) {
    //Properties are copied from the existing archive item.
    return S_OK;
  }
  switch(propID) {
    case kpidPath:   SetPropBstr(*value,item.ArcPath); break;
    case kpidIsDir:  SetPropBool(*value,item.IsDir); break;
    case kpidSize:   SetPropUInt64(*value,item.Size); break;
    case kpidAttrib: SetPropUInt32(*value,item.Attrib); break;
    case kpidMTime:  SetPropFileTime(*value,item.MTime); break;
    case kpidIsAnti: SetPropBool(*value,false); break;
    default: break;
  }
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback::GetStream(UInt32 index,ISequentialInStream **inStream))
{
  if(inStream) {
    *inStream = NULL;
  }
  if(!_items || index >= _items->size()) {
    return S_FALSE;
  }
  const COpItem &item = (*_items)[index];
  if(item.FromArchive || item.IsDir) {
    return S_OK;
  }
  try {
    if(_progress) {
      _progress->ProgressSetText(item.ArcPath);
      _progress->ProgressPump();
    }
    CMithenZipFileInStream *streamSpec = new CMithenZipFileInStream();
    CMyComPtr<ISequentialInStream> holder(streamSpec);
    if(!streamSpec->Open(item.FsPath.c_str())) {
      NumErrors++;
      return S_FALSE;
    }
    *inStream = holder.Detach();
    return S_OK;
  }
  catch(...) {
    return E_OUTOFMEMORY;
  }
}

Z7_COM7F_IMF(CUpdateCallback::SetOperationResult(Int32 /*operationResult*/))
{
  if(_progress && _progress->ProgressIsCancelled()) {
    return E_ABORT;
  }
  return S_OK;
}

Z7_COM7F_IMF(CUpdateCallback::GetVolumeSize(UInt32 /*index*/,UInt64 * /*size*/))
{
  return S_FALSE;
}

Z7_COM7F_IMF(CUpdateCallback::GetVolumeStream(UInt32 /*index*/,ISequentialOutStream **volumeStream))
{
  if(volumeStream) {
    *volumeStream = NULL;
  }
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CUpdateCallback::CryptoGetTextPassword2(Int32 *passwordIsDefined,BSTR *password))
{
  if(password) {
    *password = NULL;
  }
  if(passwordIsDefined) {
    *passwordIsDefined = _passwordDefined ? 1 : 0;
  }
  if(_passwordDefined && password) {
    BSTR b = ::SysAllocString(_password.c_str());
    if(!b) {
      return E_OUTOFMEMORY;
    }
    *password = b;
  }
  return S_OK;
}


//////////////////////////////////////////////////////////////////////////
// extract callback (extraction to disk)

class CExtractCallback Z7_final:
  public IArchiveExtractCallback,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
  Z7_IFACES_IMP_UNK_2(IArchiveExtractCallback,ICryptoGetTextPassword)
  Z7_IFACE_COM7_IMP(IProgress)

  CMyComPtr<IInArchive> _archive;
  std::wstring _outDir;
  int _pathMode;
  int _overwriteMode;
  bool _elimDup;
  std::wstring _stripPrefix;
  bool _passwordDefined;
  std::wstring _password;
  bool _wrongPassword;
  CMithenZipProgress *_progress;

  std::wstring _diskPath;
  bool _isDir;
  bool _extractMode;
  bool _hasMTime;
  FILETIME _mtime;
  bool _hasAttrib;
  UInt32 _attrib;
  CMithenZipFileOutStream *_outSpec;
  CMyComPtr<ISequentialOutStream> _out;

  HRESULT FinishOpen(const std::wstring &target,ISequentialOutStream **outStream,const std::wstring &text);

public:
  UInt64 NumErrors;

  bool WrongPassword() const { return _wrongPassword; }

  CExtractCallback():
      _pathMode(kMithenZipPathExtract_Full),
      _overwriteMode(kMithenZipOverwrite_Ask),
      _elimDup(false),
      _passwordDefined(false),
      _wrongPassword(false),
      _progress(NULL),
      _isDir(false),
      _extractMode(false),
      _hasMTime(false),
      _hasAttrib(false),
      _attrib(0),
      _outSpec(NULL),
      NumErrors(0)
  {
    _mtime.dwLowDateTime = 0;
    _mtime.dwHighDateTime = 0;
  }

  void Init(IInArchive *archive,const std::wstring &outDir,int pathMode,int overwriteMode,
      bool elimDup,const std::wstring &stripPrefix,CMithenZipProgress *progress,
      const std::wstring &password,bool passwordDefined)
  {
    _archive = archive;
    _outDir = outDir;
    if(!_outDir.empty()) {
      const wchar_t last = _outDir[_outDir.size() - 1];
      if(last != L'\\' && last != L'/') {
        _outDir += L'\\';
      }
    }
    _pathMode = pathMode;
    _overwriteMode = overwriteMode;
    _elimDup = elimDup;
    _stripPrefix = stripPrefix;
    _progress = progress;
    _password = password;
    _passwordDefined = passwordDefined;
    NumErrors = 0;
  }
};

static bool ReadItemString(IInArchive *archive,UInt32 index,PROPID propID,std::wstring &out)
{
  PROPVARIANT prop;
  ::PropVariantInit(&prop);
  bool ok = false;
  if(archive->GetProperty(index,propID,&prop) == S_OK && prop.vt == VT_BSTR && prop.bstrVal) {
    out = prop.bstrVal;
    ok = true;
  }
  ::PropVariantClear(&prop);
  return ok;
}

static bool ReadItemBool(IInArchive *archive,UInt32 index,PROPID propID,bool def)
{
  PROPVARIANT prop;
  ::PropVariantInit(&prop);
  bool result = def;
  if(archive->GetProperty(index,propID,&prop) == S_OK && prop.vt == VT_BOOL) {
    result = prop.boolVal != 0;
  }
  ::PropVariantClear(&prop);
  return result;
}

Z7_COM7F_IMF(CExtractCallback::SetTotal(UInt64 total))
{
  if(_progress) {
    _progress->ProgressSetTotal(total);
  }
  return S_OK;
}

Z7_COM7F_IMF(CExtractCallback::SetCompleted(const UInt64 *completeValue))
{
  if(_progress) {
    if(completeValue) {
      _progress->ProgressSetCompleted(*completeValue);
    }
    if(_progress->ProgressIsCancelled()) {
      return E_ABORT;
    }
  }
  return S_OK;
}

Z7_COM7F_IMF(CExtractCallback::GetStream(UInt32 index,ISequentialOutStream **outStream,Int32 askExtractMode))
{
  if(outStream) {
    *outStream = NULL;
  }
  _out.Release();
  _outSpec = NULL;
  _isDir = false;
  _extractMode = false;
  _hasMTime = false;
  _hasAttrib = false;
  _diskPath.clear();

  if(!_archive) {
    return E_FAIL;
  }
  if(askExtractMode != NArchive::NExtract::NAskMode::kExtract) {
    return S_OK;
  }
  try {
    std::wstring arcPath;
    if(!ReadItemString(_archive,index,kpidPath,arcPath)) {
      return E_FAIL;
    }
    _isDir = ReadItemBool(_archive,index,kpidIsDir,false);

    {
      PROPVARIANT prop;
      ::PropVariantInit(&prop);
      if(_archive->GetProperty(index,kpidAttrib,&prop) == S_OK && prop.vt == VT_UI4) {
        _attrib = prop.ulVal;
        _hasAttrib = true;
      }
      ::PropVariantClear(&prop);
    }
    {
      PROPVARIANT prop;
      ::PropVariantInit(&prop);
      if(_archive->GetProperty(index,kpidMTime,&prop) == S_OK && prop.vt == VT_FILETIME) {
        _mtime = prop.filetime;
        _hasMTime = true;
      }
      ::PropVariantClear(&prop);
    }

    std::wstring rel;
    if(_pathMode == kMithenZipPathExtract_No) {
      rel = MithenZip_FileName(arcPath);
    }
    else {
      rel = arcPath;
    }
    if(_elimDup && !_stripPrefix.empty()) {
      if(_wcsicmp(rel.c_str(),_stripPrefix.c_str()) == 0) {
        return S_OK;
      }
      if(rel.size() > _stripPrefix.size() &&
         _wcsnicmp(rel.c_str(),_stripPrefix.c_str(),_stripPrefix.size()) == 0 &&
         (rel[_stripPrefix.size()] == L'\\' || rel[_stripPrefix.size()] == L'/')) {
        rel = rel.substr(_stripPrefix.size() + 1);
      }
    }
    if(rel.empty()) {
      return S_OK;
    }
    const std::wstring target = JoinPath(_outDir,rel);

    if(_isDir) {
      EnsureDirectory(target);
      if(_progress) {
        _progress->ProgressSetText(rel);
        _progress->ProgressPump();
      }
      return S_OK;
    }

    EnsureDirectory(MithenZip_ParentDir(target));

    if(FileExists(target)) {
      if(_overwriteMode == kMithenZipOverwrite_Skip) {
        return S_OK;
      }
      if(_overwriteMode == kMithenZipOverwrite_Rename) {
        const std::wstring renamed = MakeUniqueName(target);
        return FinishOpen(renamed,outStream,rel);
      }
      if(_overwriteMode == kMithenZipOverwrite_RenameExisting) {
        ::MoveFileW(target.c_str(),MakeUniqueName(target).c_str());
      }
      else if(_overwriteMode == kMithenZipOverwrite_Ask) {
        const HWND owner = _progress ? _progress->ProgressOwner() : NULL;
        const int answer = ::MessageBoxW(owner,
            (L"File already exists:\n" + target + L"\n\nOverwrite?").c_str(),
            L"MithenZip",MB_YESNO | MB_ICONQUESTION);
        if(answer != IDYES) {
          return S_OK;
        }
      }
    }
    return FinishOpen(target,outStream,rel);
  }
  catch(...) {
    return E_OUTOFMEMORY;
  }
}

HRESULT CExtractCallback::FinishOpen(const std::wstring &target,ISequentialOutStream **outStream,
    const std::wstring &text)
{
  try {
    CMithenZipFileOutStream *streamSpec = new CMithenZipFileOutStream();
    CMyComPtr<ISequentialOutStream> holder(streamSpec);
    if(!streamSpec->Create(target.c_str())) {
      NumErrors++;
      return S_OK;
    }
    _outSpec = streamSpec;
    _out = holder;
    _diskPath = target;
    if(_progress) {
      _progress->ProgressSetText(text);
      _progress->ProgressPump();
    }
    *outStream = holder.Detach();
    _extractMode = true;
    return S_OK;
  }
  catch(...) {
    return E_OUTOFMEMORY;
  }
}

Z7_COM7F_IMF(CExtractCallback::PrepareOperation(Int32 askExtractMode))
{
  _extractMode = (askExtractMode == NArchive::NExtract::NAskMode::kExtract);
  return S_OK;
}

Z7_COM7F_IMF(CExtractCallback::SetOperationResult(Int32 operationResult))
{
  if(operationResult != NArchive::NExtract::NOperationResult::kOK) {
    NumErrors++;
  }
  const bool okResult = (operationResult == NArchive::NExtract::NOperationResult::kOK);
  if(_outSpec) {
    if(okResult && _hasMTime) {
      _outSpec->SetMTime(_mtime);
    }
    _outSpec->Close();
  }
  _out.Release();
  _outSpec = NULL;
  if(okResult) {
    if(_extractMode && _hasAttrib && !_diskPath.empty()) {
      if((_attrib & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        ::SetFileAttributesW(_diskPath.c_str(),_attrib);
      }
    }
  }
  else {
    //Never leave a partially written or corrupt file behind.
    if(_extractMode && !_diskPath.empty()) {
      ::SetFileAttributesW(_diskPath.c_str(),FILE_ATTRIBUTE_NORMAL);
      ::DeleteFileW(_diskPath.c_str());
    }
    if(operationResult == NArchive::NExtract::NOperationResult::kWrongPassword) {
      _wrongPassword = true;
      return E_ABORT;  //stop the extraction immediately
    }
  }
  if(_progress && _progress->ProgressIsCancelled()) {
    return E_ABORT;
  }
  return S_OK;
}

Z7_COM7F_IMF(CExtractCallback::CryptoGetTextPassword(BSTR *password))
{
  if(password) {
    *password = NULL;
  }
  if(!_passwordDefined) {
    return E_ABORT;
  }
  BSTR b = ::SysAllocString(_password.c_str());
  if(!b) {
    return E_OUTOFMEMORY;
  }
  *password = b;
  return S_OK;
}


//////////////////////////////////////////////////////////////////////////
// compression options -> ISetProperties

static bool FormatAllows(const std::wstring &format,const wchar_t *name)
{
  const bool is7z = (_wcsicmp(format.c_str(),L"7z") == 0);
  const bool isZip = (_wcsicmp(format.c_str(),L"zip") == 0);
  const bool isXz = (_wcsicmp(format.c_str(),L"xz") == 0);
  const bool isTar = (_wcsicmp(format.c_str(),L"tar") == 0);
  const bool isBzip2 = (_wcsicmp(format.c_str(),L"bzip2") == 0);
  if(wcscmp(name,L"x") == 0) {
    return true;
  }
  if(wcscmp(name,L"mt") == 0) {
    return is7z || isZip || isBzip2 || isXz;
  }
  if(wcscmp(name,L"m") == 0) {
    return is7z || isZip || isXz || isTar;
  }
  if(wcscmp(name,L"d") == 0) {
    return is7z || isZip || isXz;
  }
  if(wcscmp(name,L"fb") == 0) {
    return is7z || isZip || isXz;
  }
  if(wcscmp(name,L"s") == 0) {
    return is7z || isXz;
  }
  if(wcscmp(name,L"he") == 0) {
    return is7z;
  }
  if(wcscmp(name,L"em") == 0) {
    return is7z;
  }
  return false;
}

static void AddPropU32(std::vector<const wchar_t*> &names,std::vector<PROPVARIANT> &values,
    const wchar_t *name,UInt32 value)
{
  PROPVARIANT p;
  ::PropVariantInit(&p);
  SetPropUInt32(p,value);
  names.push_back(name);
  values.push_back(p);
}

static void AddPropStr(std::vector<const wchar_t*> &names,std::vector<PROPVARIANT> &values,
    const wchar_t *name,const std::wstring &value)
{
  PROPVARIANT p;
  ::PropVariantInit(&p);
  SetPropBstr(p,value);
  names.push_back(name);
  values.push_back(p);
}

static void FreeProps(std::vector<PROPVARIANT> &values)
{
  for(size_t i = 0; i < values.size(); i++) {
    if(values[i].vt == VT_BSTR && values[i].bstrVal) {
      ::SysFreeString(values[i].bstrVal);
    }
  }
  values.clear();
}

static void ApplyCompressProperties(IOutArchive *outArchive,const CMithenZipCompressOptions &options)
{
  CMyComPtr<ISetProperties> setProperties;
  if(outArchive->QueryInterface(IID_ISetProperties,(void**)&setProperties) != S_OK || !setProperties) {
    return;
  }
  const std::wstring &format = options.FormatName;
  const UInt32 level = (UInt32)((options.Level < 0) ? 5 : options.Level);

  std::vector<const wchar_t*> names;
  std::vector<PROPVARIANT> values;

  AddPropU32(names,values,L"x",level);
  if(!options.Method.empty() && FormatAllows(format,L"m")) {
    AddPropStr(names,values,L"m",options.Method);
  }
  if(options.Dictionary != 0 && FormatAllows(format,L"d")) {
    AddPropU32(names,values,L"d",(UInt32)options.Dictionary);
  }
  if(options.WordSize != 0 && FormatAllows(format,L"fb")) {
    AddPropU32(names,values,L"fb",options.WordSize);
  }
  if(options.SolidBlock != 0 && FormatAllows(format,L"s")) {
    AddPropU32(names,values,L"s",(UInt32)options.SolidBlock);
  }
  if(options.NumThreads > 0 && FormatAllows(format,L"mt")) {
    AddPropU32(names,values,L"mt",(UInt32)options.NumThreads);
  }
  if(options.EncryptFileNames && FormatAllows(format,L"he")) {
    AddPropStr(names,values,L"he",L"on");
  }
  if(!options.EncryptionMethod.empty() && FormatAllows(format,L"em")) {
    AddPropStr(names,values,L"em",options.EncryptionMethod);
  }

  HRESULT result = setProperties->SetProperties(&names[0],&values[0],(UInt32)names.size());
  if(result != S_OK) {
    //Fall back to the universally supported level property only.
    FreeProps(values);
    names.clear();
    AddPropU32(names,values,L"x",level);
    setProperties->SetProperties(&names[0],&values[0],1);
  }
  FreeProps(values);
}


//////////////////////////////////////////////////////////////////////////
// operations

HRESULT MithenZip_Compress(CMithenZipProgress *progress,
    const std::vector<std::wstring> &inputPaths,
    const std::wstring &archivePath,
    const CMithenZipCompressOptions &options)
{
  if(inputPaths.empty() || archivePath.empty()) {
    return E_INVALIDARG;
  }
  std::wstring tempPath;
  try {
    CMithenZipFormatRegistry &registry = CMithenZipFormatRegistry::Instance();
    if(!registry.Ready()) {
      return E_FAIL;
    }
    CMithenZipFormat format;
    if(!registry.FindFormatByName(options.FormatName,format) || !format.Update) {
      return E_INVALIDARG;
    }
    Func_CreateObject createObject = registry.CreateObject();
    if(!createObject) {
      return E_FAIL;
    }

    std::vector<COpItem> newItems;
    BuildNewItems(inputPaths,options.PathMode,archivePath,newItems);
    if(newItems.empty()) {
      return E_FAIL;
    }

    //Open the existing archive for Update/Freshen (Add just overwrites).
    CMithenZipArchivePtr existing;
    std::vector<COpItem> items;
    const bool wantUpdate = (options.UpdateMode != kMithenZipUpdate_Add);
    if(wantUpdate && FileExists(archivePath)) {
      CMithenZipArchive *archive = new CMithenZipArchive();
      existing = CMithenZipArchivePtr(archive);
      archive->Release();
      if(existing->Open(archivePath.c_str()) && existing->Archive()) {
        const std::vector<CMithenZipArcItem> &old = existing->Items();
        for(size_t i = 0; i < old.size(); i++) {
          COpItem item;
          item.FromArchive = true;
          item.ArchiveIndex = (UInt32)i;
          item.ArcPath = old[i].Path;
          item.IsDir = old[i].IsDir;
          item.MTime = old[i].MTime;
          items.push_back(item);
        }
      }
    }

    if(options.UpdateMode == kMithenZipUpdate_Fresh && !items.empty()) {
      //Only replace existing items whose source is newer; never add new items.
      for(size_t i = 0; i < newItems.size(); i++) {
        const COpItem &ni = newItems[i];
        for(size_t k = 0; k < items.size(); k++) {
          if(_wcsicmp(items[k].ArcPath.c_str(),ni.ArcPath.c_str()) == 0) {
            if(::CompareFileTime(&ni.MTime,&items[k].MTime) > 0) {
              COpItem replacement = ni;
              replacement.FromArchive = false;
              items[k] = replacement;
            }
            break;
          }
        }
      }
    }
    else {
      for(size_t i = 0; i < newItems.size(); i++) {
        const COpItem &ni = newItems[i];
        bool replaced = false;
        for(size_t k = 0; k < items.size(); k++) {
          if(_wcsicmp(items[k].ArcPath.c_str(),ni.ArcPath.c_str()) == 0) {
            items[k] = ni;
            replaced = true;
            break;
          }
        }
        if(!replaced) {
          items.push_back(ni);
        }
      }
      if(items.empty()) {
        items = newItems;
      }
    }
    if(items.empty()) {
      return E_FAIL;
    }

    CMyComPtr<IOutArchive> outArchive;
    if(!items.empty() && existing.IsDefined() && existing->Archive()) {
      existing->Archive()->QueryInterface(IID_IOutArchive,(void**)&outArchive);
    }
    if(!outArchive) {
      if(createObject(&format.Clsid,&IID_IOutArchive,(void**)&outArchive) != S_OK) {
        return E_FAIL;
      }
    }

    ApplyCompressProperties(outArchive,options);

    tempPath = archivePath + L".z7tmp";
    CMithenZipFileOutStream *outSpec = new CMithenZipFileOutStream();
    CMyComPtr<IOutStream> outStream(outSpec);
    if(!outSpec->Create(tempPath.c_str())) {
      const HRESULT openError = HRESULT_FROM_WIN32(::GetLastError());
      return openError;
    }

    CUpdateCallback *callbackSpec = new CUpdateCallback();
    CMyComPtr<IArchiveUpdateCallback2> callback(callbackSpec);
    callbackSpec->Init(items,progress,options.Password,!options.Password.empty());

    const UInt32 numItems = (UInt32)items.size();
    HRESULT result = outArchive->UpdateItems(outStream,numItems,callback);
    outSpec->Close();

    if(result == S_OK && callbackSpec->NumErrors != 0) {
      result = E_FAIL;
    }
    if(result == S_OK) {
      if(!::MoveFileExW(tempPath.c_str(),archivePath.c_str(),
          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        result = HRESULT_FROM_WIN32(::GetLastError());
      }
    }
    if(result != S_OK) {
      ::DeleteFileW(tempPath.c_str());
      return result;
    }

    if(options.DeleteAfter) {
      for(size_t i = 0; i < inputPaths.size(); i++) {
        DeleteTree(inputPaths[i]);
      }
    }
    return S_OK;
  }
  catch(...) {
    if(!tempPath.empty()) {
      ::DeleteFileW(tempPath.c_str());
    }
    return E_OUTOFMEMORY;
  }
}

static std::wstring FirstComponent(const std::wstring &path)
{
  const size_t sep = path.find(L'\\');
  if(sep == std::wstring::npos) {
    return path;
  }
  return path.substr(0,sep);
}

static std::wstring CommonRootComponent(const std::vector<CMithenZipArcItem> &items)
{
  if(items.empty()) {
    return std::wstring();
  }
  const std::wstring root = FirstComponent(items[0].Path);
  if(root.empty()) {
    return std::wstring();
  }
  for(size_t i = 0; i < items.size(); i++) {
    const std::wstring &path = items[i].Path;
    if(_wcsicmp(path.c_str(),root.c_str()) == 0) {
      //The root entry itself must be a folder; a single top-level file is not a folder to strip.
      if(!items[i].IsDir) {
        return std::wstring();
      }
      continue;
    }
    if(_wcsicmp(FirstComponent(path).c_str(),root.c_str()) != 0) {
      return std::wstring();
    }
    if(path.size() <= root.size() || (path[root.size()] != L'\\' && path[root.size()] != L'/')){
      return std::wstring();
    }
  }
  return root;
}

HRESULT MithenZip_Extract(CMithenZipProgress *progress,
    const std::wstring &archivePath,
    const CMithenZipExtractOptions &options)
{
  if(archivePath.empty() || options.OutputDir.empty()) {
    return E_INVALIDARG;
  }
  try {
    CMithenZipArchivePtr archive;
    {
      CMithenZipArchive *a = new CMithenZipArchive();
      archive = CMithenZipArchivePtr(a);
      a->Release();
    }
    if(!archive->Open(archivePath.c_str()) || !archive->Archive()) {
      return E_FAIL;
    }

    std::wstring password = options.Password;
    if(password.empty() && archive->IsEncrypted()) {
      HWND owner = progress ? progress->ProgressOwner() : NULL;
      if(!owner) {
        owner = MithenZip_PromptOwner();
      }
      std::wstring entered;
      if(MithenZip_ShowPasswordDialog(owner,entered)) {
        password = entered;
      }
    }

    std::wstring stripPrefix;
    if(options.ElimDup) {
      stripPrefix = CommonRootComponent(archive->Items());
    }

    CExtractCallback *callbackSpec = new CExtractCallback();
    CMyComPtr<IArchiveExtractCallback> callback(callbackSpec);
    callbackSpec->Init(archive->Archive(),options.OutputDir,options.PathMode,
        options.OverwriteMode,options.ElimDup,stripPrefix,progress,
        password,!password.empty());

    const HRESULT result = archive->Archive()->Extract(NULL,(UInt32)(Int32)-1,false,callback);
    if(callbackSpec->WrongPassword()) {
      return MITHENZIP_E_WRONG_PASSWORD;
    }
    if(result != S_OK) {
      return result;
    }
    if(callbackSpec->NumErrors != 0) {
      return E_FAIL;
    }
    return S_OK;
  }
  catch(...) {
    return E_OUTOFMEMORY;
  }
}
