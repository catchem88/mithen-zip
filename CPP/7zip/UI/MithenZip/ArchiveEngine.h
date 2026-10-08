// ArchiveEngine.h

#ifndef ZIP7_INC_MITHENZIP_ARCHIVEENGINE_H
#define ZIP7_INC_MITHENZIP_ARCHIVEENGINE_H

#include "../../../Common/Common0.h"
#include "../../../Common/MyCom.h"
#include "../../IStream.h"
#include "../../Archive/IArchive.h"
#include "../../IPassword.h"

#include <vector>
#include <string>

struct CMithenZipArcItem
{
  std::wstring Path;
  UInt64 Size;
  UInt64 PackSize;
  FILETIME MTime;
  UInt32 Attrib;
  bool IsDir;
  bool HasMTime;
};

struct CMithenZipFormat
{
  std::wstring Name;
  GUID Clsid;
  std::wstring Extensions;
  bool Update;
};

//Process-wide registry of every archive format exposed by 7z.dll.
class CMithenZipFormatRegistry
{
  bool _ready;
  HMODULE _lib;
  Func_CreateObject _createObject;
  Func_GetNumberOfFormats _getNumberOfFormats;
  Func_GetHandlerProperty2 _getHandlerProperty2;
  Func_GetIsArc _getIsArc;
  std::vector<CMithenZipFormat> _formats;

  Z7_CLASS_NO_COPY(CMithenZipFormatRegistry)

  CMithenZipFormatRegistry();
  ~CMithenZipFormatRegistry();
  bool Load();

public:
  static CMithenZipFormatRegistry& Instance();

  bool Ready() const { return _ready; }
  Func_CreateObject CreateObject() const { return _createObject; }
  const std::vector<CMithenZipFormat>& Formats() const { return _formats; }
  bool FindFormatByName(const std::wstring &name,CMithenZipFormat &out) const;
  void GetWritableFormatNames(std::vector<std::wstring> &out) const;

  //Ordered candidate format indices for the given header bytes and candidate
  //extensions (most specific first, e.g. "7z" then "001" for "sample.7z.001").
  void GetCandidates(const Byte *header,size_t headerSize,
      const std::vector<std::wstring> &extensions,std::vector<UINT> &out) const;
};

//Curated archive file extensions that this shell extension associates and handles.
const wchar_t * const * MithenZip_ArchiveExtensions(size_t &count);
const wchar_t * const * MithenZip_DroppedExtensions(size_t &count);

//True for image files Windows mounts itself (.iso/.img/.vhd/.vhdx).
bool MithenZip_IsImageExtension(const std::wstring &extension);

//True when the Windows archive handlers can browse this type.
bool MithenZip_IsWindowsOpenableExtension(const std::wstring &extension);

//Types claimed on install (supported minus the ones Windows opens itself).
const wchar_t * const * MithenZip_AssociatedExtensions(size_t &count);
bool MithenZip_IsArchiveExtension(const std::wstring &extension);

class CMithenZipArchive
{
  LONG _ref;
  CMyComPtr<IInArchive> _archive;
  std::vector<CMithenZipArcItem> _items;
  std::wstring _filePath;
  CRITICAL_SECTION _cs;
  std::wstring _password;
  bool _passwordDefined;

  Z7_CLASS_NO_COPY(CMithenZipArchive)

  bool TryOpenWith(const GUID &clsid,const std::wstring &filePath);
  void ReadItems();

public:
  CMithenZipArchive();
  ~CMithenZipArchive();

  void AddRef();
  void Release();

  bool Open(LPCWSTR filePath);
  void Close();

  bool IsOpen() const { return _archive != NULL; }
  const std::wstring& FilePath() const { return _filePath; }
  const std::vector<CMithenZipArcItem>& Items() const { return _items; }
  IInArchive* Archive() const { return _archive; }

  HRESULT ExtractToMemory(UInt32 index,std::vector<Byte> &data);

  void SetPassword(const std::wstring &password);
  bool HasPassword() const { return _passwordDefined; }
  bool IsEncrypted() const;
};

//Curated archive file extensions that this shell extension associates and handles.
const wchar_t * const * MithenZip_ArchiveExtensions(size_t &count);
const wchar_t * const * MithenZip_DroppedExtensions(size_t &count);

//True for image files Windows mounts itself (.iso/.img/.vhd/.vhdx).
bool MithenZip_IsImageExtension(const std::wstring &extension);

//True when the Windows archive handlers can browse this type.
bool MithenZip_IsWindowsOpenableExtension(const std::wstring &extension);

//Types claimed on install (supported minus the ones Windows opens itself).
const wchar_t * const * MithenZip_AssociatedExtensions(size_t &count);
bool MithenZip_IsArchiveExtension(const std::wstring &extension);

class CMithenZipArchivePtr
{
  CMithenZipArchive *_p;
public:
  CMithenZipArchivePtr(): _p(NULL) {}
  explicit CMithenZipArchivePtr(CMithenZipArchive *p): _p(p) { if(_p) _p->AddRef(); }
  CMithenZipArchivePtr(const CMithenZipArchivePtr &o): _p(o._p) { if(_p) _p->AddRef(); }
  ~CMithenZipArchivePtr() { if(_p) _p->Release(); }

  CMithenZipArchivePtr& operator=(const CMithenZipArchivePtr &o)
  {
    if(o._p) {
      o._p->AddRef();
    }
    if(_p) {
      _p->Release();
    }
    _p = o._p;
    return *this;
  }

  CMithenZipArchive* operator->() const { return _p; }
  CMithenZipArchive* Get() const { return _p; }
  bool IsDefined() const { return _p != NULL; }
  explicit operator bool() const { return _p != NULL; }
};

#endif
