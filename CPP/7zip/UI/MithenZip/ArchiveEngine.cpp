// ArchiveEngine.cpp

#include "StdAfx.h"

#include "../../../Common/MyCom.h"
#include "../../../Common/ComTry.h"

#include "../../IStream.h"
#include "../../Archive/IArchive.h"
#include "../../IPassword.h"

#include "MithenZip.h"
#include "ArchiveEngine.h"
#include "Streams.h"

Z7_DIAGNOSTIC_IGNORE_CAST_FUNCTION

static const wchar_t * const k7zDllName = L"7z.dll";

static void PropInit(PROPVARIANT &p)
{
  memset(&p,0,sizeof(p));
  p.vt = VT_EMPTY;
}

static void PropFree(PROPVARIANT &p)
{
  if(p.vt == VT_BSTR && p.bstrVal) {
    ::SysFreeString(p.bstrVal);
  }
  memset(&p,0,sizeof(p));
  p.vt = VT_EMPTY;
}

static UInt64 PropGetUInt64(const PROPVARIANT &p,UInt64 def)
{
  switch(p.vt) {
    case VT_UI8: return (UInt64)p.uhVal.QuadPart;
    case VT_UI4: return (UInt64)p.ulVal;
    case VT_I4:  return (UInt64)(Int64)p.lVal;
    case VT_I8:  return (UInt64)p.hVal.QuadPart;
    default:     return def;
  }
}

static bool PropGetBool(const PROPVARIANT &p,bool def)
{
  if(p.vt == VT_BOOL) {
    return p.boolVal != 0;
  }
  return def;
}

static std::wstring GetDirPrefix(const std::wstring &path)
{
  const size_t pos = path.find_last_of(L'\\');
  if(pos == std::wstring::npos) {
    return std::wstring();
  }
  return path.substr(0,pos + 1);
}

static std::wstring GetBaseName(const std::wstring &path)
{
  const size_t pos = path.find_last_of(L'\\');
  if(pos == std::wstring::npos) {
    return path;
  }
  return path.substr(pos + 1);
}

static std::wstring ToLower(const std::wstring &text)
{
  std::wstring result = text;
  for(size_t i = 0; i < result.size(); i++) {
    if(result[i] >= L'A' && result[i] <= L'Z') {
      result[i] = (WCHAR)(result[i] - L'A' + L'a');
    }
  }
  return result;
}

static std::wstring FileNameOnly(const std::wstring &path)
{
  const size_t sep = path.find_last_of(L"\\/");
  if(sep == std::wstring::npos) {
    return path;
  }
  return path.substr(sep + 1);
}

static std::wstring LastExtension(const std::wstring &name)
{
  const size_t dot = name.find_last_of(L'.');
  if(dot == std::wstring::npos || dot + 1 >= name.size()) {
    return std::wstring();
  }
  return ToLower(name.substr(dot));
}

static bool IsNumericExtension(const std::wstring &ext)
{
  if(ext.size() < 2 || ext[0] != L'.') {
    return false;
  }
  for(size_t i = 1; i < ext.size(); i++) {
    if(ext[i] < L'0' || ext[i] > L'9') {
      return false;
    }
  }
  return true;
}

//Candidate extensions for a file, most specific first. For a volume like
//"sample.7z.001" the underlying "7z" is tried before the "001" volume suffix.
static void FileExtensions(const std::wstring &path,std::vector<std::wstring> &out)
{
  out.clear();
  const std::wstring name = FileNameOnly(path);
  const std::wstring ext = LastExtension(name);
  if(ext.empty()) {
    return;
  }
  if(IsNumericExtension(ext)) {
    const std::wstring base = name.substr(0,name.size() - ext.size());
    const std::wstring underlying = LastExtension(base);
    if(!underlying.empty()) {
      out.push_back(underlying);
    }
  }
  out.push_back(ext);
}

static bool FormatMatchesExtension(const CMithenZipFormat &format,const std::wstring &extension)
{
  if(extension.size() < 2 || extension[0] != L'.') {
    return false;
  }
  const std::wstring token = extension.substr(1);
  const std::wstring &list = format.Extensions;
  size_t pos = 0;
  while(pos < list.size()) {
    while(pos < list.size() && list[pos] == L' ') {
      pos++;
    }
    const size_t start = pos;
    while(pos < list.size() && list[pos] != L' ') {
      pos++;
    }
    if(pos > start && _wcsicmp(list.substr(start,pos - start).c_str(),token.c_str()) == 0) {
      return true;
    }
  }
  return false;
}

static bool ReadHeader(const std::wstring &path,std::vector<Byte> &header)
{
  header.clear();
  HANDLE file = ::CreateFileW(path.c_str(),GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file == INVALID_HANDLE_VALUE) {
    return false;
  }
  const DWORD wanted = 1 << 16;
  header.resize(wanted);
  DWORD read = 0;
  const bool ok = ::ReadFile(file,&header[0],wanted,&read,NULL) && read > 0;
  ::CloseHandle(file);
  if(!ok) {
    header.clear();
    return false;
  }
  header.resize(read);
  return true;
}

static bool ReadFormatString(Func_GetHandlerProperty2 getProp,UInt32 index,PROPID propID,std::wstring &out)
{
  PROPVARIANT prop;
  PropInit(prop);
  bool ok = false;
  if(getProp(index,propID,&prop) == S_OK && prop.vt == VT_BSTR && prop.bstrVal) {
    out = prop.bstrVal;
    ok = true;
  }
  PropFree(prop);
  return ok;
}

static bool ReadFormatGuid(Func_GetHandlerProperty2 getProp,UInt32 index,GUID &out)
{
  PROPVARIANT prop;
  PropInit(prop);
  bool ok = false;
  if(getProp(index,NArchive::NHandlerPropID::kClassID,&prop) == S_OK && prop.vt == VT_BSTR && prop.bstrVal) {
    if(::SysStringByteLen(prop.bstrVal) == sizeof(GUID)) {
      memcpy(&out,prop.bstrVal,sizeof(GUID));
      ok = true;
    }
  }
  PropFree(prop);
  return ok;
}


class CMithenZipOpenCallback Z7_final:
  public IArchiveOpenCallback,
  public IArchiveOpenVolumeCallback,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
  Z7_IFACES_IMP_UNK_3(IArchiveOpenCallback,IArchiveOpenVolumeCallback,ICryptoGetTextPassword)

public:
  bool PasswordDefined;
  std::wstring Password;
  std::wstring DirPrefix;
  std::wstring BaseName;

  CMithenZipOpenCallback(): PasswordDefined(false) {}
};

Z7_COM7F_IMF(CMithenZipOpenCallback::SetTotal(const UInt64 * /*files*/,const UInt64 * /*bytes*/))
{
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipOpenCallback::SetCompleted(const UInt64 * /*files*/,const UInt64 * /*bytes*/))
{
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipOpenCallback::GetProperty(PROPID propID,PROPVARIANT *value))
{
  if(!value) {
    return E_POINTER;
  }
  if(propID == kpidName) {
    BSTR b = ::SysAllocString(BaseName.c_str());
    if(!b) {
      return E_OUTOFMEMORY;
    }
    value->vt = VT_BSTR;
    value->bstrVal = b;
  }
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipOpenCallback::GetStream(const wchar_t *name,IInStream **inStream))
{
  if(inStream) {
    *inStream = NULL;
  }
  if(!name || !inStream) {
    return S_FALSE;
  }
  try {
    const std::wstring path = DirPrefix + name;
    CMithenZipFileInStream *streamSpec = new CMithenZipFileInStream();
    CMyComPtr<IInStream> stream = streamSpec;
    if(!streamSpec->Open(path.c_str())) {
      return S_FALSE;
    }
    *inStream = stream.Detach();
    return S_OK;
  }
  catch(...) {
    return E_OUTOFMEMORY;
  }
}

Z7_COM7F_IMF(CMithenZipOpenCallback::CryptoGetTextPassword(BSTR *password))
{
  if(!PasswordDefined) {
    return E_ABORT;
  }
  BSTR b = ::SysAllocString(Password.c_str());
  if(!b) {
    return E_OUTOFMEMORY;
  }
  if(password) {
    *password = b;
  }
  else {
    ::SysFreeString(b);
  }
  return S_OK;
}


class CMithenZipMemOutStream Z7_final: public ISequentialOutStream,public CMyUnknownImp
{
  Z7_IFACES_IMP_UNK_1(ISequentialOutStream)

public:
  std::vector<Byte> Data;
};

Z7_COM7F_IMF(CMithenZipMemOutStream::Write(const void *data,UInt32 size,UInt32 *processedSize))
{
  try {
    if(size) {
      const Byte *p = (const Byte*)data;
      Data.insert(Data.end(),p,p + size);
    }
    if(processedSize) {
      *processedSize = size;
    }
    return S_OK;
  }
  catch(...) {
    if(processedSize) {
      *processedSize = 0;
    }
    return E_OUTOFMEMORY;
  }
}


class CMithenZipExtractCallback Z7_final:
  public IArchiveExtractCallback,
  public ICryptoGetTextPassword,
  public CMyUnknownImp
{
  Z7_IFACES_IMP_UNK_2(IArchiveExtractCallback,ICryptoGetTextPassword)
  Z7_IFACE_COM7_IMP(IProgress)

  CMyComPtr<IInArchive> _archive;
  UInt32 _targetIndex;
  CMithenZipMemOutStream *_streamSpec;
  CMyComPtr<ISequentialOutStream> _stream;
  bool _passwordDefined;
  std::wstring _password;

public:
  CMithenZipExtractCallback(): _targetIndex((UInt32)(Int32)-1),_streamSpec(NULL),_passwordDefined(false) {}

  void Init(IInArchive *archive,UInt32 index,const std::wstring &password,bool passwordDefined)
  {
    _archive = archive;
    _targetIndex = index;
    _password = password;
    _passwordDefined = passwordDefined;
  }

  bool GetData(std::vector<Byte> &out) const
  {
    if(!_streamSpec) {
      return false;
    }
    out = _streamSpec->Data;
    return true;
  }
};

Z7_COM7F_IMF(CMithenZipExtractCallback::SetTotal(UInt64 /*total*/))
{
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipExtractCallback::SetCompleted(const UInt64 * /*completeValue*/))
{
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipExtractCallback::GetStream(UInt32 index,
    ISequentialOutStream **outStream,Int32 askExtractMode))
{
  if(outStream) {
    *outStream = NULL;
  }
  _stream.Release();
  if(index != _targetIndex || askExtractMode != NArchive::NExtract::NAskMode::kExtract) {
    return S_OK;
  }
  try {
    _streamSpec = new CMithenZipMemOutStream();
    CMyComPtr<ISequentialOutStream> holder(_streamSpec);
    _stream = holder;
    *outStream = holder.Detach();
    return S_OK;
  }
  catch(...) {
    return E_OUTOFMEMORY;
  }
}

Z7_COM7F_IMF(CMithenZipExtractCallback::PrepareOperation(Int32 /*askExtractMode*/))
{
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipExtractCallback::SetOperationResult(Int32 /*operationResult*/))
{
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipExtractCallback::CryptoGetTextPassword(BSTR *password))
{
  if(!_passwordDefined) {
    return E_ABORT;
  }
  BSTR b = ::SysAllocString(_password.c_str());
  if(!b) {
    return E_OUTOFMEMORY;
  }
  if(password) {
    *password = b;
  }
  else {
    ::SysFreeString(b);
  }
  return S_OK;
}


CMithenZipFormatRegistry& CMithenZipFormatRegistry::Instance()
{
  static CMithenZipFormatRegistry instance;
  return instance;
}

CMithenZipFormatRegistry::CMithenZipFormatRegistry():
    _ready(false),
    _lib(NULL),
    _createObject(NULL),
    _getNumberOfFormats(NULL),
    _getHandlerProperty2(NULL),
    _getIsArc(NULL)
{
  _ready = Load();
}

CMithenZipFormatRegistry::~CMithenZipFormatRegistry()
{
  if(_lib) {
    ::FreeLibrary(_lib);
  }
}

bool CMithenZipFormatRegistry::Load()
{
  HMODULE lib = NULL;
  wchar_t modulePath[MAX_PATH];
  modulePath[0] = 0;
  if(g_hInstance && ::GetModuleFileNameW(g_hInstance,modulePath,MAX_PATH)) {
    std::wstring full = modulePath;
    const size_t pos = full.find_last_of(L'\\');
    if(pos != std::wstring::npos) {
      full.erase(pos + 1);
      full += k7zDllName;
      lib = ::LoadLibraryExW(full.c_str(),NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
    }
  }
  if(!lib) {
    lib = ::LoadLibraryW(k7zDllName);
  }
  if(!lib) {
    return false;
  }
  _createObject = Z7_GET_PROC_ADDRESS(Func_CreateObject,lib,"CreateObject");
  _getNumberOfFormats = Z7_GET_PROC_ADDRESS(Func_GetNumberOfFormats,lib,"GetNumberOfFormats");
  _getHandlerProperty2 = Z7_GET_PROC_ADDRESS(Func_GetHandlerProperty2,lib,"GetHandlerProperty2");
  _getIsArc = Z7_GET_PROC_ADDRESS(Func_GetIsArc,lib,"GetIsArc");
  if(!_createObject || !_getNumberOfFormats || !_getHandlerProperty2) {
    ::FreeLibrary(lib);
    return false;
  }
  UInt32 count = 0;
  if(_getNumberOfFormats(&count) != S_OK || count == 0) {
    ::FreeLibrary(lib);
    return false;
  }
  _formats.reserve(count);
  for(UInt32 i = 0; i < count; i++) {
    CMithenZipFormat format;
    format.Update = false;
    if(!ReadFormatString(_getHandlerProperty2,i,NArchive::NHandlerPropID::kName,format.Name)) {
      continue;
    }
    if(!ReadFormatGuid(_getHandlerProperty2,i,format.Clsid)) {
      continue;
    }
    std::wstring extensions;
    ReadFormatString(_getHandlerProperty2,i,NArchive::NHandlerPropID::kExtension,extensions);
    std::wstring add;
    if(ReadFormatString(_getHandlerProperty2,i,NArchive::NHandlerPropID::kAddExtension,add) && !add.empty()) {
      if(!extensions.empty()) {
        extensions += L' ';
      }
      extensions += add;
    }
    format.Extensions = ToLower(extensions);
    PROPVARIANT prop;
    PropInit(prop);
    if(_getHandlerProperty2(i,NArchive::NHandlerPropID::kUpdate,&prop) == S_OK) {
      format.Update = PropGetBool(prop,false);
    }
    PropFree(prop);
    _formats.push_back(format);
  }
  _lib = lib;
  return !_formats.empty();
}

void CMithenZipFormatRegistry::GetCandidates(const Byte *header,size_t headerSize,
    const std::vector<std::wstring> &extensions,std::vector<UINT> &out) const
{
  out.clear();
  std::vector<UINT> strong;
  std::vector<UINT> weak;
  std::vector<UINT> weakest;
  for(UINT i = 0; i < (UINT)_formats.size(); i++) {
    const CMithenZipFormat &format = _formats[i];
    bool signatureMatch = false;
    if(_getIsArc && header && headerSize) {
      Func_IsArc isArc = NULL;
      if(_getIsArc(i,&isArc) == S_OK && isArc && isArc(header,headerSize) != 0) {
        signatureMatch = true;
      }
    }
    bool extensionMatch = false;
    for(size_t k = 0; k < extensions.size() && !extensionMatch; k++) {
      if(FormatMatchesExtension(format,extensions[k])) {
        extensionMatch = true;
      }
    }
    if(format.Name == L"Split") {
      //The Split handler only concatenates volumes; keep it as a last resort.
      if(extensionMatch) {
        weakest.push_back(i);
      }
      continue;
    }
    if(signatureMatch) {
      strong.push_back(i);
    }
    else if(extensionMatch) {
      weak.push_back(i);
    }
  }
  for(size_t i = 0; i < strong.size(); i++) {
    out.push_back(strong[i]);
  }
  for(size_t i = 0; i < weak.size(); i++) {
    out.push_back(weak[i]);
  }
  for(size_t i = 0; i < weakest.size(); i++) {
    out.push_back(weakest[i]);
  }
  if(out.empty()) {
    for(UINT i = 0; i < (UINT)_formats.size(); i++) {
      out.push_back(i);
    }
  }
}


CMithenZipArchive::CMithenZipArchive(): _ref(1),_passwordDefined(false)
{
  ::InitializeCriticalSection(&_cs);
}

CMithenZipArchive::~CMithenZipArchive()
{
  Close();
  ::DeleteCriticalSection(&_cs);
}

void CMithenZipArchive::AddRef()
{
  ::InterlockedIncrement(&_ref);
}

void CMithenZipArchive::Release()
{
  if(::InterlockedDecrement(&_ref) == 0) {
    delete this;
  }
}

bool CMithenZipArchive::TryOpenWith(const GUID &clsid,const std::wstring &filePath)
{
  CMithenZipFormatRegistry &registry = CMithenZipFormatRegistry::Instance();
  Func_CreateObject createObject = registry.CreateObject();
  if(!createObject) {
    return false;
  }
  CMyComPtr<IInArchive> archive;
  if(createObject(&clsid,&IID_IInArchive,(void**)&archive) != S_OK) {
    return false;
  }
  CMithenZipFileInStream *fileSpec = new CMithenZipFileInStream();
  CMyComPtr<IInStream> file = fileSpec;
  if(!fileSpec->Open(filePath.c_str())) {
    return false;
  }
  CMithenZipOpenCallback *callbackSpec = new CMithenZipOpenCallback();
  CMyComPtr<IArchiveOpenCallback> callback = callbackSpec;
  callbackSpec->DirPrefix = GetDirPrefix(filePath);
  callbackSpec->BaseName = GetBaseName(filePath);
  callbackSpec->PasswordDefined = _passwordDefined;
  callbackSpec->Password = _password;
  const UInt64 scanSize = (UInt64)1 << 23;
  if(archive->Open(file,&scanSize,callback) != S_OK) {
    return false;
  }
  _archive = archive;
  return true;
}

void CMithenZipArchive::ReadItems()
{
  _items.clear();
  UInt32 numItems = 0;
  if(_archive->GetNumberOfItems(&numItems) != S_OK) {
    return;
  }
  _items.reserve(numItems);
  for(UInt32 i = 0; i < numItems; i++) {
    CMithenZipArcItem item;
    item.Size = 0;
    item.PackSize = 0;
    item.Attrib = 0;
    item.IsDir = false;
    item.HasMTime = false;
    item.MTime.dwLowDateTime = 0;
    item.MTime.dwHighDateTime = 0;

    PROPVARIANT prop;
    PropInit(prop);
    if(_archive->GetProperty(i,kpidPath,&prop) == S_OK && prop.vt == VT_BSTR && prop.bstrVal) {
      item.Path = prop.bstrVal;
    }
    PropFree(prop);

    for(size_t k = 0; k < item.Path.size(); k++) {
      if(item.Path[k] == L'/') {
        item.Path[k] = L'\\';
      }
    }
    while(!item.Path.empty() && item.Path[0] == L'\\') {
      item.Path.erase(item.Path.begin());
    }
    while(!item.Path.empty() && item.Path[item.Path.size() - 1] == L'\\') {
      item.Path.erase(item.Path.size() - 1);
    }
    if(item.Path.empty()) {
      continue;
    }

    PropInit(prop);
    if(_archive->GetProperty(i,kpidSize,&prop) == S_OK) {
      item.Size = PropGetUInt64(prop,0);
    }
    PropFree(prop);

    PropInit(prop);
    if(_archive->GetProperty(i,kpidPackSize,&prop) == S_OK) {
      item.PackSize = PropGetUInt64(prop,0);
    }
    PropFree(prop);

    PropInit(prop);
    if(_archive->GetProperty(i,kpidAttrib,&prop) == S_OK) {
      item.Attrib = (UInt32)PropGetUInt64(prop,0);
    }
    PropFree(prop);

    PropInit(prop);
    if(_archive->GetProperty(i,kpidMTime,&prop) == S_OK && prop.vt == VT_FILETIME) {
      item.MTime = prop.filetime;
      item.HasMTime = true;
    }
    PropFree(prop);

    PropInit(prop);
    if(_archive->GetProperty(i,kpidIsDir,&prop) == S_OK) {
      item.IsDir = PropGetBool(prop,false);
    }
    PropFree(prop);

    _items.push_back(item);
  }
}

void CMithenZipArchive::SetPassword(const std::wstring &password)
{
  _password = password;
  _passwordDefined = !password.empty();
}

bool CMithenZipArchive::IsEncrypted() const
{
  if(!_archive) {
    return false;
  }
  PROPVARIANT prop;
  PropInit(prop);
  bool encrypted = false;
  if(_archive->GetArchiveProperty(kpidEncrypted,&prop) == S_OK) {
    encrypted = PropGetBool(prop,false);
  }
  PropFree(prop);
  if(!encrypted) {
    //7-Zip sets kpidEncrypted on encrypted items rather than the archive.
    UInt32 numItems = 0;
    if(_archive->GetNumberOfItems(&numItems) == S_OK) {
      for(UInt32 i = 0; i < numItems && !encrypted; i++) {
        PropInit(prop);
        if(_archive->GetProperty(i,kpidEncrypted,&prop) == S_OK) {
          encrypted = PropGetBool(prop,false);
        }
        PropFree(prop);
      }
    }
  }
  return encrypted;
}

bool CMithenZipArchive::Open(LPCWSTR filePath)
{
  if(!filePath || !filePath[0]) {
    return false;
  }
  Close();
  CMithenZipFormatRegistry &registry = CMithenZipFormatRegistry::Instance();
  if(!registry.Ready()) {
    return false;
  }
  const std::wstring path = filePath;
  std::vector<Byte> header;
  ReadHeader(path,header);
  std::vector<std::wstring> extensions;
  FileExtensions(path,extensions);
  std::vector<UINT> candidates;
  registry.GetCandidates(header.empty() ? NULL : &header[0],header.size(),extensions,candidates);
  const std::vector<CMithenZipFormat> &formats = registry.Formats();
  for(size_t i = 0; i < candidates.size(); i++) {
    if(candidates[i] >= formats.size()) {
      continue;
    }
    if(TryOpenWith(formats[candidates[i]].Clsid,path)) {
      _filePath = path;
      ReadItems();
      return true;
    }
  }
  return false;
}

void CMithenZipArchive::Close()
{
  _archive.Release();
  _items.clear();
  _filePath.clear();
}

HRESULT CMithenZipArchive::ExtractToMemory(UInt32 index,std::vector<Byte> &data)
{
  data.clear();
  if(!_archive) {
    return E_FAIL;
  }
  ::EnterCriticalSection(&_cs);
  HRESULT result = E_FAIL;
  try {
    CMithenZipExtractCallback *callbackSpec = new CMithenZipExtractCallback();
    CMyComPtr<IArchiveExtractCallback> callback = callbackSpec;
    callbackSpec->Init(_archive,index,_password,_passwordDefined);
    UInt32 itemIndex = index;
    result = _archive->Extract(&itemIndex,1,false,callback);
    if(result == S_OK) {
      callbackSpec->GetData(data);
    }
  }
  catch(...) {
    result = E_OUTOFMEMORY;
  }
  ::LeaveCriticalSection(&_cs);
  return result;
}


bool CMithenZipFormatRegistry::FindFormatByName(const std::wstring &name,CMithenZipFormat &out) const
{
  for(size_t i = 0; i < _formats.size(); i++) {
    if(_wcsicmp(_formats[i].Name.c_str(),name.c_str()) == 0) {
      out = _formats[i];
      return true;
    }
  }
  return false;
}

void CMithenZipFormatRegistry::GetWritableFormatNames(std::vector<std::wstring> &out) const
{
  static const wchar_t * const kPreferred[] = {L"zip",L"7z",L"tar",L"gzip",L"bzip2",L"xz",L"wim"};
  out.clear();
  for(size_t k = 0; k < sizeof(kPreferred) / sizeof(kPreferred[0]); k++) {
    for(size_t i = 0; i < _formats.size(); i++) {
      if(_formats[i].Update && _wcsicmp(_formats[i].Name.c_str(),kPreferred[k]) == 0) {
        out.push_back(_formats[i].Name);
        break;
      }
    }
  }
}

static const wchar_t * const kMithenZipArchiveExtensions[] = {
  L".7z",L".rar",L".zip",L".zipx",L".tar",L".gz",L".tgz",L".xz",L".txz",
  L".bz2",L".tbz",L".tbz2",L".cab",L".iso",L".wim",L".swm",L".esd",
  L".lzh",L".lha",L".z",L".taz",L".cpio",L".arj",L".rpm",L".deb",
  L".lzma",L".lz",L".zst",L".001"
};

const wchar_t * const * MithenZip_ArchiveExtensions(size_t &count)
{
  count = sizeof(kMithenZipArchiveExtensions) / sizeof(kMithenZipArchiveExtensions[0]);
  return kMithenZipArchiveExtensions;
}

//Extensions earlier builds claimed but no longer do. The system default is better
//for them: Windows mounts .iso images itself.
static const wchar_t * const kMithenZipDroppedExtensions[] = {
  L".iso"
};

const wchar_t * const * MithenZip_DroppedExtensions(size_t &count)
{
  count = sizeof(kMithenZipDroppedExtensions) / sizeof(kMithenZipDroppedExtensions[0]);
  return kMithenZipDroppedExtensions;
}

//Types Windows opens properly by itself (it mounts these), so we do not claim them.
static const wchar_t * const kMithenZipImageExtensions[] = {
  L".iso",L".img",L".vhd",L".vhdx"
};

bool MithenZip_IsImageExtension(const std::wstring &extension)
{
  if(extension.empty()) {
    return false;
  }
  for(size_t i = 0; i < sizeof(kMithenZipImageExtensions) / sizeof(kMithenZipImageExtensions[0]); i++) {
    if(_wcsicmp(extension.c_str(),kMithenZipImageExtensions[i]) == 0) {
      return true;
    }
  }
  return false;
}

//Types the Windows archive handlers (CompressedFolder / ArchiveFolder) can browse.
static const wchar_t * const kMithenZipWindowsOpenable[] = {
  L".zip",L".7z",L".rar",L".tar",L".gz",L".tgz",L".xz",L".txz",
  L".bz2",L".tbz2",L".cpio",L".zst",L".cab"
};

bool MithenZip_IsWindowsOpenableExtension(const std::wstring &extension)
{
  if(extension.empty()) {
    return false;
  }
  for(size_t i = 0; i < sizeof(kMithenZipWindowsOpenable) / sizeof(kMithenZipWindowsOpenable[0]); i++) {
    if(_wcsicmp(extension.c_str(),kMithenZipWindowsOpenable[i]) == 0) {
      return true;
    }
  }
  return false;
}

//Types claimed on install: every supported type except the ones Windows opens itself.
static const wchar_t * const kMithenZipAssociatedExtensions[] = {
  L".7z",L".rar",L".zip",L".zipx",L".tar",L".gz",L".tgz",L".xz",L".txz",
  L".bz2",L".tbz",L".tbz2",L".cab",L".wim",L".swm",L".esd",
  L".lzh",L".lha",L".z",L".taz",L".cpio",L".arj",L".rpm",L".deb",
  L".lzma",L".lz",L".zst",L".001"
};

const wchar_t * const * MithenZip_AssociatedExtensions(size_t &count)
{
  count = sizeof(kMithenZipAssociatedExtensions) / sizeof(kMithenZipAssociatedExtensions[0]);
  return kMithenZipAssociatedExtensions;
}

bool MithenZip_IsArchiveExtension(const std::wstring &extension)
{
  if(extension.empty()) {
    return false;
  }
  const size_t count = sizeof(kMithenZipArchiveExtensions) / sizeof(kMithenZipArchiveExtensions[0]);
  for(size_t i = 0; i < count; i++) {
    if(_wcsicmp(kMithenZipArchiveExtensions[i],extension.c_str()) == 0) {
      return true;
    }
  }
  return false;
}
