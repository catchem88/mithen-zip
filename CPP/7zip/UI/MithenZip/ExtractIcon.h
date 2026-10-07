// ExtractIcon.h

#ifndef ZIP7_INC_MITHENZIP_EXTRACTICON_H
#define ZIP7_INC_MITHENZIP_EXTRACTICON_H

#include "../../../Common/MyWindows.h"
#include <shlobj.h>

#include <string>

class CMithenZipExtractIcon Z7_final: public IExtractIconW
{
  LONG _ref;
  std::wstring _name;
  bool _isDir;

  Z7_CLASS_NO_COPY(CMithenZipExtractIcon)

public:
  CMithenZipExtractIcon();
  ~CMithenZipExtractIcon();

  void Init(const std::wstring &name,bool isDir)
  {
    _name = name;
    _isDir = isDir;
  }

  STDMETHOD(QueryInterface)(REFIID riid,void **ppv);
  STDMETHOD_(ULONG,AddRef)();
  STDMETHOD_(ULONG,Release)();
  STDMETHOD(GetIconLocation)(UINT uFlags,LPWSTR szIconFile,UINT cchMax,int *piIndex,UINT *pwFlags);
  STDMETHOD(Extract)(LPCWSTR pszFile,UINT nIconIndex,HICON *phiconLarge,HICON *phiconSmall,UINT nIconSize);
};

#endif
