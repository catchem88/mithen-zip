// ExtractIcon.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "ExtractIcon.h"

CMithenZipExtractIcon::CMithenZipExtractIcon(): _ref(1),_isDir(false)
{
}

CMithenZipExtractIcon::~CMithenZipExtractIcon()
{
}

STDMETHODIMP CMithenZipExtractIcon::QueryInterface(REFIID riid,void **ppv)
{
  if(!ppv) {
    return E_POINTER;
  }
  *ppv = NULL;
  if(riid == IID_IUnknown || riid == IID_IExtractIconW || riid == IID_IExtractIcon) {
    *ppv = static_cast<IExtractIconW*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CMithenZipExtractIcon::AddRef()
{
  return (ULONG)::InterlockedIncrement(&_ref);
}

STDMETHODIMP_(ULONG) CMithenZipExtractIcon::Release()
{
  const LONG value = ::InterlockedDecrement(&_ref);
  if(value == 0) {
    delete this;
  }
  return (ULONG)value;
}

STDMETHODIMP CMithenZipExtractIcon::GetIconLocation(UINT /*uFlags*/,LPWSTR szIconFile,
    UINT cchMax,int *piIndex,UINT *pwFlags)
{
  COM_TRY_BEGIN
  if(piIndex) {
    *piIndex = 0;
  }
  if(pwFlags) {
    *pwFlags = 0;
  }
  if(!szIconFile || cchMax == 0) {
    return E_POINTER;
  }
  SHFILEINFOW info;
  memset(&info,0,sizeof(info));
  const DWORD attributes = _isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
  const std::wstring templateName = _isDir ? std::wstring(L"folder") : _name;
  const DWORD_PTR result = ::SHGetFileInfoW(templateName.c_str(),attributes,
      &info,sizeof(info),SHGFI_ICONLOCATION | SHGFI_USEFILEATTRIBUTES);
  if(result == 0) {
    return S_FALSE;
  }
  wcsncpy_s(szIconFile,cchMax,info.szDisplayName,_TRUNCATE);
  if(piIndex) {
    *piIndex = info.iIcon;
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CMithenZipExtractIcon::Extract(LPCWSTR /*pszFile*/,UINT /*nIconIndex*/,
    HICON * /*phiconLarge*/,HICON * /*phiconSmall*/,UINT /*nIconSize*/)
{
  //Let the Shell load the icon from the location returned by GetIconLocation.
  return S_FALSE;
}
