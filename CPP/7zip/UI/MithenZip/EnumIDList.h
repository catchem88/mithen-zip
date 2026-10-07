// EnumIDList.h

#ifndef ZIP7_INC_MITHENZIP_ENUMIDLIST_H
#define ZIP7_INC_MITHENZIP_ENUMIDLIST_H

#include "../../../Common/MyWindows.h"
#include <shlobj.h>

#include <vector>

class CMithenZipEnumIDList Z7_final: public IEnumIDList
{
  LONG _ref;
  std::vector<LPITEMIDLIST> _pidls;
  size_t _pos;

public:
  CMithenZipEnumIDList(): _ref(1),_pos(0) {}
  ~CMithenZipEnumIDList();

  void Init(std::vector<LPITEMIDLIST> &pidls) { _pidls.swap(pidls); }

  STDMETHOD(QueryInterface)(REFIID riid,void **ppv);
  STDMETHOD_(ULONG,AddRef)();
  STDMETHOD_(ULONG,Release)();

  STDMETHOD(Next)(ULONG celt,LPITEMIDLIST *rgelt,ULONG *pceltFetched);
  STDMETHOD(Skip)(ULONG celt);
  STDMETHOD(Reset)();
  STDMETHOD(Clone)(IEnumIDList **ppenum);
};

#endif
