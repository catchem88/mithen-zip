// ShellFolder.h

#ifndef ZIP7_INC_MITHENZIP_SHELLFOLDER_H
#define ZIP7_INC_MITHENZIP_SHELLFOLDER_H

#include "../../../Common/MyWindows.h"
#include <shlobj.h>

#include "MithenZip.h"
#include "ArchiveEngine.h"

class CMithenZipFolder Z7_final: public IShellFolder2,public IPersistFolder2
{
  LONG _ref;
  CMithenZipArchivePtr _archive;
  std::wstring _innerPath;
  std::wstring _displayName;
  PIDLIST_ABSOLUTE _fullPidl;

  Z7_CLASS_NO_COPY(CMithenZipFolder)

public:
  CMithenZipFolder();
  ~CMithenZipFolder();

  STDMETHOD(QueryInterface)(REFIID riid,void **ppv);
  STDMETHOD_(ULONG,AddRef)();
  STDMETHOD_(ULONG,Release)();

  STDMETHOD(GetClassID)(CLSID *pClassID);
  STDMETHOD(Initialize)(PCIDLIST_ABSOLUTE pidl);
  STDMETHOD(GetCurFolder)(PIDLIST_ABSOLUTE *ppidl);

  STDMETHOD(ParseDisplayName)(HWND hwnd,IBindCtx *pbc,LPWSTR pszDisplayName,ULONG *pchEaten,PIDLIST_RELATIVE *ppidl,ULONG *pdwAttributes);
  STDMETHOD(EnumObjects)(HWND hwnd,SHCONTF grfFlags,IEnumIDList **ppenum);
  STDMETHOD(BindToObject)(PCUIDLIST_RELATIVE pidl,IBindCtx *pbc,REFIID riid,void **ppv);
  STDMETHOD(BindToStorage)(PCUIDLIST_RELATIVE pidl,IBindCtx *pbc,REFIID riid,void **ppv);
  STDMETHOD(CompareIDs)(LPARAM lParam,PCUIDLIST_RELATIVE pidl1,PCUIDLIST_RELATIVE pidl2);
  STDMETHOD(CreateViewObject)(HWND hwndOwner,REFIID riid,void **ppv);
  STDMETHOD(GetAttributesOf)(UINT cidl,PCUITEMID_CHILD_ARRAY apidl,SFGAOF *rgfInOut);
  STDMETHOD(GetUIObjectOf)(HWND hwndOwner,UINT cidl,PCUITEMID_CHILD_ARRAY apidl,REFIID riid,UINT *rgfReserved,void **ppv);
  STDMETHOD(GetDisplayNameOf)(PCUITEMID_CHILD pidl,SHGDNF uFlags,LPSTRRET str);
  STDMETHOD(SetNameOf)(HWND hwnd,PCUITEMID_CHILD pidl,LPCWSTR pszName,SHGDNF uFlags,PITEMID_CHILD *ppidlOut);

  STDMETHOD(GetDefaultSearchGUID)(GUID *pguid);
  STDMETHOD(EnumSearches)(IEnumExtraSearch **ppenum);
  STDMETHOD(GetDefaultColumn)(DWORD dwReserved,ULONG *pSort,ULONG *pDisplay);
  STDMETHOD(GetDefaultColumnState)(UINT iColumn,SHCOLSTATEF *pcsFlags);
  STDMETHOD(GetDetailsEx)(PCUITEMID_CHILD pidl,const SHCOLUMNID *pscid,VARIANT *pv);
  STDMETHOD(GetDetailsOf)(PCUITEMID_CHILD pidl,UINT iColumn,SHELLDETAILS *psd);
  STDMETHOD(MapColumnToSCID)(UINT iColumn,SHCOLUMNID *pscid);

  const CMithenZipArchivePtr& Archive() const { return _archive; }
  const std::wstring& InnerPath() const { return _innerPath; }
};

#endif
