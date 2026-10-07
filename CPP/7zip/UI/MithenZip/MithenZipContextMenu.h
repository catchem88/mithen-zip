// ContextMenu.h

#ifndef ZIP7_INC_MITHENZIP_CONTEXTMENU_H
#define ZIP7_INC_MITHENZIP_CONTEXTMENU_H

#include "../../../Common/MyWindows.h"
#include <shlobj.h>

#include <string>
#include <vector>

enum CMithenZipCommand
{
  kMithenZipCmd_Popup = -1,
  kMithenZipCmd_AddToArchive = 0,
  kMithenZipCmd_AddToZip,
  kMithenZipCmd_AddTo7z,
  kMithenZipCmd_Open,
  kMithenZipCmd_ExtractFiles,
  kMithenZipCmd_ExtractHere
};

class CMithenZipContextMenu Z7_final:
  public IContextMenu,
  public IShellExtInit
{
  LONG _ref;
  std::vector<std::wstring> _fileNames;
  std::vector<int> _commands;   //parallel to the returned menu ids
  UINT _idCmdFirst;
  bool _hasDirectories;
  bool _hasArchive;

  Z7_CLASS_NO_COPY(CMithenZipContextMenu)

  HRESULT InvokeCommandCommon(int command);

public:
  CMithenZipContextMenu();
  ~CMithenZipContextMenu();

  //IUnknown
  STDMETHOD(QueryInterface)(REFIID riid,void **ppv);
  STDMETHOD_(ULONG,AddRef)();
  STDMETHOD_(ULONG,Release)();

  //IShellExtInit
  STDMETHOD(Initialize)(LPCITEMIDLIST pidlFolder,LPDATAOBJECT dataObject,HKEY hkeyProgID);

  //IContextMenu
  STDMETHOD(QueryContextMenu)(HMENU hMenu,UINT indexMenu,UINT idCmdFirst,UINT idCmdLast,UINT uFlags);
  STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO pici);
  STDMETHOD(GetCommandString)(UINT_PTR idCmd,UINT uType,UINT *pwReserved,LPSTR pszName,UINT cchMax);
};

#endif
