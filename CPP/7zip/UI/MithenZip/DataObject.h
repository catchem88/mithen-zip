// DataObject.h

#ifndef ZIP7_INC_MITHENZIP_DATAOBJECT_H
#define ZIP7_INC_MITHENZIP_DATAOBJECT_H

#include "../../../Common/MyWindows.h"
#include <objidl.h>
#include <ole2.h>
#include <shlobj.h>

#include "MithenZip.h"
#include "ArchiveEngine.h"

class CMithenZipDataObject Z7_final: public IDataObject
{
  LONG _ref;
  CMithenZipArchivePtr _archive;
  std::vector<std::wstring> _names;
  std::vector<bool> _isDir;
  std::vector<UInt32> _indices;

  Z7_CLASS_NO_COPY(CMithenZipDataObject)

  void AddEntry(const std::wstring &relativeName,const std::wstring &fullPath,bool isDir);

public:
  CMithenZipDataObject();
  ~CMithenZipDataObject();

  void Init(const CMithenZipArchivePtr &archive,const std::wstring &innerPath,
      UINT cidl,PCUITEMID_CHILD_ARRAY apidl);

  STDMETHOD(QueryInterface)(REFIID riid,void **ppv);
  STDMETHOD_(ULONG,AddRef)();
  STDMETHOD_(ULONG,Release)();
  STDMETHOD(GetData)(FORMATETC *pformatetcIn,STGMEDIUM *pmedium);
  STDMETHOD(GetDataHere)(FORMATETC *pformatetc,STGMEDIUM *pmedium);
  STDMETHOD(QueryGetData)(FORMATETC *pformatetc);
  STDMETHOD(GetCanonicalFormatEtc)(FORMATETC *pformatetcIn,FORMATETC *pformatetcOut);
  STDMETHOD(SetData)(FORMATETC *pformatetc,STGMEDIUM *pmedium,BOOL fRelease);
  STDMETHOD(EnumFormatEtc)(DWORD dwDirection,IEnumFORMATETC **ppenumFormatEtc);
  STDMETHOD(DAdvise)(FORMATETC *pformatetc,DWORD advf,IAdviseSink *pAdvSink,DWORD *pdwConnection);
  STDMETHOD(DUnadvise)(DWORD dwConnection);
  STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppenumAdvise);
};

#endif
