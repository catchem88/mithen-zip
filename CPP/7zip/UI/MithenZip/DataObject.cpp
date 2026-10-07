// DataObject.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "Pidl.h"
#include "DataObject.h"
#include "Dialogs.h"

static UINT GetFormatFileGroupDescriptor()
{
  static UINT format = 0;
  if(!format) {
    format = ::RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
  }
  return format;
}

static UINT GetFormatFileContents()
{
  static UINT format = 0;
  if(!format) {
    format = ::RegisterClipboardFormatW(CFSTR_FILECONTENTS);
  }
  return format;
}

static UINT GetFormatPreferredDropEffect()
{
  static UINT format = 0;
  if(!format) {
    format = ::RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
  }
  return format;
}


class CMithenZipEnumFormatEtc Z7_final: public IEnumFORMATETC
{
  LONG _ref;
  UINT _pos;

public:
  CMithenZipEnumFormatEtc(): _ref(1),_pos(0) {}

  STDMETHOD(QueryInterface)(REFIID riid,void **ppv)
  {
    if(!ppv) {
      return E_POINTER;
    }
    *ppv = NULL;
    if(riid == IID_IUnknown || riid == IID_IEnumFORMATETC) {
      *ppv = static_cast<IEnumFORMATETC*>(this);
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

  STDMETHOD(Next)(ULONG celt,FORMATETC *rgelt,ULONG *pceltFetched)
  {
    if(pceltFetched) {
      *pceltFetched = 0;
    }
    if(!rgelt) {
      return E_POINTER;
    }
    ULONG count = 0;
    while(count < celt && _pos < 3) {
      FORMATETC &format = rgelt[count];
      memset(&format,0,sizeof(format));
      format.dwAspect = DVASPECT_CONTENT;
      format.lindex = -1;
      if(_pos == 0) {
        format.cfFormat = (CLIPFORMAT)GetFormatFileGroupDescriptor();
        format.tymed = TYMED_HGLOBAL;
      }
      else if(_pos == 1) {
        format.cfFormat = (CLIPFORMAT)GetFormatFileContents();
        format.tymed = TYMED_ISTREAM;
      }
      else {
        format.cfFormat = (CLIPFORMAT)GetFormatPreferredDropEffect();
        format.tymed = TYMED_HGLOBAL;
      }
      count++;
      _pos++;
    }
    if(pceltFetched) {
      *pceltFetched = count;
    }
    if(count == celt) {
      return S_OK;
    }
    return S_FALSE;
  }

  STDMETHOD(Skip)(ULONG celt)
  {
    _pos += celt;
    if(_pos >= 3) {
      _pos = 3;
      return S_FALSE;
    }
    return S_OK;
  }

  STDMETHOD(Reset)()
  {
    _pos = 0;
    return S_OK;
  }

  STDMETHOD(Clone)(IEnumFORMATETC **ppenum)
  {
    if(!ppenum) {
      return E_POINTER;
    }
    *ppenum = NULL;
    CMithenZipEnumFormatEtc *copy = new CMithenZipEnumFormatEtc();
    copy->_pos = _pos;
    *ppenum = copy;
    return S_OK;
  }
};


CMithenZipDataObject::CMithenZipDataObject(): _ref(1)
{
  ::InterlockedIncrement(&g_dllRefCount);
}

CMithenZipDataObject::~CMithenZipDataObject()
{
  ::InterlockedDecrement(&g_dllRefCount);
}

void CMithenZipDataObject::AddEntry(const std::wstring &relativeName,
    const std::wstring &fullPath,bool isDir)
{
  UInt32 index = (UInt32)(Int32)-1;
  if(!isDir && _archive) {
    const std::vector<CMithenZipArcItem> &items = _archive->Items();
    for(size_t i = 0; i < items.size(); i++) {
      if(_wcsicmp(items[i].Path.c_str(),fullPath.c_str()) == 0) {
        index = (UInt32)i;
        break;
      }
    }
  }
  _names.push_back(relativeName);
  _isDir.push_back(isDir);
  _indices.push_back(index);
}

void CMithenZipDataObject::Init(const CMithenZipArchivePtr &archive,
    const std::wstring &innerPath,UINT cidl,PCUITEMID_CHILD_ARRAY apidl)
{
  _archive = archive;
  _names.clear();
  _isDir.clear();
  _indices.clear();
  for(UINT i = 0; i < cidl; i++) {
    if(!apidl[i] || !MithenZip_PidlIsOurs(apidl[i])) {
      continue;
    }
    const std::wstring name = MithenZip_PidlName(apidl[i]);
    const bool isDir = MithenZip_PidlIsDir(apidl[i]);
    const std::wstring fullPath = innerPath + name;
    AddEntry(fullPath,fullPath,isDir);
    if(isDir && _archive) {
      const std::wstring prefix = fullPath + L"\\";
      const std::vector<CMithenZipArcItem> &items = _archive->Items();
      for(size_t k = 0; k < items.size(); k++) {
        if(items[k].Path.size() > prefix.size() && items[k].Path.compare(0,prefix.size(),prefix) == 0) {
          AddEntry(items[k].Path,items[k].Path,items[k].IsDir);
        }
      }
    }
  }
}

STDMETHODIMP CMithenZipDataObject::QueryInterface(REFIID riid,void **ppv)
{
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  if(riid == IID_IUnknown || riid == IID_IDataObject) {
    *ppv = static_cast<IDataObject*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CMithenZipDataObject::AddRef()
{
  return (ULONG)::InterlockedIncrement(&_ref);
}

STDMETHODIMP_(ULONG) CMithenZipDataObject::Release()
{
  const LONG value = ::InterlockedDecrement(&_ref);
  if(value == 0) {
    delete this;
  }
  return (ULONG)value;
}

STDMETHODIMP CMithenZipDataObject::GetData(FORMATETC *pformatetcIn,STGMEDIUM *pmedium)
{
  COM_TRY_BEGIN
  if(!pformatetcIn || !pmedium) {
    return E_POINTER;
  }
  memset(pmedium,0,sizeof(*pmedium));
  if(pformatetcIn->cfFormat == (CLIPFORMAT)GetFormatFileGroupDescriptor()) {
    if(!(pformatetcIn->tymed & TYMED_HGLOBAL)) {
      return DV_E_TYMED;
    }
    const size_t count = _names.size();
    const size_t size = sizeof(UINT) + count * sizeof(FILEDESCRIPTORW);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,size);
    if(!memory) {
      return E_OUTOFMEMORY;
    }
    FILEGROUPDESCRIPTORW *descriptor = (FILEGROUPDESCRIPTORW*)::GlobalLock(memory);
    if(!descriptor) {
      ::GlobalFree(memory);
      return E_OUTOFMEMORY;
    }
    descriptor->cItems = (UINT)count;
    for(size_t i = 0; i < count; i++) {
      FILEDESCRIPTORW &entry = descriptor->fgd[i];
      wcsncpy_s(entry.cFileName,MAX_PATH,_names[i].c_str(),_TRUNCATE);
      if(_isDir[i]) {
        entry.dwFlags = FD_ATTRIBUTES;
        entry.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        continue;
      }
      entry.dwFlags = FD_ATTRIBUTES | FD_FILESIZE;
      entry.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
      const CMithenZipArcItem *item = NULL;
      const UInt32 index = _indices[i];
      if(_archive && index != (UInt32)(Int32)-1 && index < _archive->Items().size()) {
        item = &_archive->Items()[index];
      }
      const UInt64 itemSize = item ? item->Size : 0;
      entry.nFileSizeHigh = (DWORD)(itemSize >> 32);
      entry.nFileSizeLow = (DWORD)(itemSize & 0xFFFFFFFF);
      if(item && item->HasMTime) {
        entry.dwFlags |= FD_WRITESTIME;
        entry.ftLastWriteTime = item->MTime;
      }
    }
    ::GlobalUnlock(memory);
    pmedium->tymed = TYMED_HGLOBAL;
    pmedium->hGlobal = memory;
    pmedium->pUnkForRelease = NULL;
    return S_OK;
  }
  if(pformatetcIn->cfFormat == (CLIPFORMAT)GetFormatFileContents()) {
    if(!(pformatetcIn->tymed & TYMED_ISTREAM)) {
      return DV_E_TYMED;
    }
    const LONG index = pformatetcIn->lindex;
    if(index < 0 || (size_t)index >= _names.size()) {
      return DV_E_LINDEX;
    }
    if(!_archive) {
      return E_UNEXPECTED;
    }
    std::vector<Byte> data;
    if(!_isDir[index]) {
      const UInt32 itemIndex = _indices[index];
      if(itemIndex == (UInt32)(Int32)-1) {
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
      }
      HRESULT result = _archive->ExtractToMemory(itemIndex,data);
      if(result != S_OK && !_archive->HasPassword() && (result == E_ABORT || _archive->IsEncrypted())) {
        std::wstring password;
        if(MithenZip_ShowPasswordDialog(MithenZip_PromptOwner(),password)) {
          _archive->SetPassword(password);
          result = _archive->ExtractToMemory(itemIndex,data);
        }
      }
      if(result != S_OK) {
        return result;
      }
    }
    if(data.size() > 0xFFFFFFFFu) {
      return E_OUTOFMEMORY;
    }
    IStream *stream = ::SHCreateMemStream(data.empty() ? NULL : &data[0],(UINT)data.size());
    if(!stream) {
      return E_OUTOFMEMORY;
    }
    pmedium->tymed = TYMED_ISTREAM;
    pmedium->pstm = stream;
    pmedium->pUnkForRelease = NULL;
    return S_OK;
  }
  if(pformatetcIn->cfFormat == (CLIPFORMAT)GetFormatPreferredDropEffect()) {
    if(!(pformatetcIn->tymed & TYMED_HGLOBAL)) {
      return DV_E_TYMED;
    }
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,sizeof(DWORD));
    if(!memory) {
      return E_OUTOFMEMORY;
    }
    DWORD *effect = (DWORD*)::GlobalLock(memory);
    if(effect) {
      *effect = DROPEFFECT_COPY;
      ::GlobalUnlock(memory);
    }
    pmedium->tymed = TYMED_HGLOBAL;
    pmedium->hGlobal = memory;
    pmedium->pUnkForRelease = NULL;
    return S_OK;
  }
  return DV_E_FORMATETC;
  COM_TRY_END
}

STDMETHODIMP CMithenZipDataObject::GetDataHere(FORMATETC * /*pformatetc*/,STGMEDIUM * /*pmedium*/)
{
  return E_NOTIMPL;
}

STDMETHODIMP CMithenZipDataObject::QueryGetData(FORMATETC *pformatetc)
{
  if(!pformatetc) {
    return E_POINTER;
  }
  if(pformatetc->cfFormat == (CLIPFORMAT)GetFormatFileGroupDescriptor()) {
    return (pformatetc->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_TYMED;
  }
  if(pformatetc->cfFormat == (CLIPFORMAT)GetFormatFileContents()) {
    return (pformatetc->tymed & TYMED_ISTREAM) ? S_OK : DV_E_TYMED;
  }
  if(pformatetc->cfFormat == (CLIPFORMAT)GetFormatPreferredDropEffect()) {
    return (pformatetc->tymed & TYMED_HGLOBAL) ? S_OK : DV_E_TYMED;
  }
  return DV_E_FORMATETC;
}

STDMETHODIMP CMithenZipDataObject::GetCanonicalFormatEtc(FORMATETC * /*pformatetcIn*/,
    FORMATETC * /*pformatetcOut*/)
{
  return E_NOTIMPL;
}

STDMETHODIMP CMithenZipDataObject::SetData(FORMATETC * /*pformatetc*/,
    STGMEDIUM * /*pmedium*/,BOOL /*fRelease*/)
{
  return E_NOTIMPL;
}

STDMETHODIMP CMithenZipDataObject::EnumFormatEtc(DWORD dwDirection,IEnumFORMATETC **ppenumFormatEtc)
{
  if(!ppenumFormatEtc) {
    return E_POINTER;
  }
  *ppenumFormatEtc = NULL;
  if(dwDirection != DATADIR_GET) {
    return E_NOTIMPL;
  }
  *ppenumFormatEtc = new CMithenZipEnumFormatEtc();
  return S_OK;
}

STDMETHODIMP CMithenZipDataObject::DAdvise(FORMATETC * /*pformatetc*/,DWORD /*advf*/,
    IAdviseSink * /*pAdvSink*/,DWORD * /*pdwConnection*/)
{
  return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP CMithenZipDataObject::DUnadvise(DWORD /*dwConnection*/)
{
  return OLE_E_ADVISENOTSUPPORTED;
}

STDMETHODIMP CMithenZipDataObject::EnumDAdvise(IEnumSTATDATA ** /*ppenumAdvise*/)
{
  return OLE_E_ADVISENOTSUPPORTED;
}
