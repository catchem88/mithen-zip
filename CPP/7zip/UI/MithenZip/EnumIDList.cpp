// EnumIDList.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "Pidl.h"
#include "EnumIDList.h"

CMithenZipEnumIDList::~CMithenZipEnumIDList()
{
  for(size_t i = 0; i < _pidls.size(); i++) {
    if(_pidls[i]) {
      CoTaskMemFree(_pidls[i]);
    }
  }
}

STDMETHODIMP CMithenZipEnumIDList::QueryInterface(REFIID riid,void **ppv)
{
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  if(riid == IID_IUnknown || riid == IID_IEnumIDList) {
    *ppv = static_cast<IEnumIDList*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CMithenZipEnumIDList::AddRef()
{
  return (ULONG)::InterlockedIncrement(&_ref);
}

STDMETHODIMP_(ULONG) CMithenZipEnumIDList::Release()
{
  const LONG value = ::InterlockedDecrement(&_ref);
  if(value == 0) {
    delete this;
  }
  return (ULONG)value;
}

STDMETHODIMP CMithenZipEnumIDList::Next(ULONG celt,LPITEMIDLIST *rgelt,ULONG *pceltFetched)
{
  COM_TRY_BEGIN
  if(pceltFetched) {
    *pceltFetched = 0;
  }
  if(!rgelt) {
    return E_POINTER;
  }
  if(celt != 1 && !pceltFetched) {
    return E_INVALIDARG;
  }
  ULONG count = 0;
  while(count < celt && _pos < _pidls.size()) {
    rgelt[count] = MithenZip_PidlCopy(_pidls[_pos]);
    if(!rgelt[count]) {
      break;
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
  COM_TRY_END
}

STDMETHODIMP CMithenZipEnumIDList::Skip(ULONG celt)
{
  _pos += celt;
  if(_pos >= _pidls.size()) {
    _pos = _pidls.size();
    return S_FALSE;
  }
  return S_OK;
}

STDMETHODIMP CMithenZipEnumIDList::Reset()
{
  _pos = 0;
  return S_OK;
}

STDMETHODIMP CMithenZipEnumIDList::Clone(IEnumIDList **ppenum)
{
  COM_TRY_BEGIN
  if(!ppenum) {
    return E_POINTER;
  }
  *ppenum = NULL;
  CMithenZipEnumIDList *copy = new CMithenZipEnumIDList();
  for(size_t i = 0; i < _pidls.size(); i++) {
    LPITEMIDLIST item = MithenZip_PidlCopy(_pidls[i]);
    if(item) {
      copy->_pidls.push_back(item);
    }
  }
  copy->_pos = _pos;
  *ppenum = copy;
  return S_OK;
  COM_TRY_END
}
