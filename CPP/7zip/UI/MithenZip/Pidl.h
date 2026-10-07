// Pidl.h

#ifndef ZIP7_INC_MITHENZIP_PIDL_H
#define ZIP7_INC_MITHENZIP_PIDL_H

#include "../../../Common/MyWindows.h"
#include <shlobj.h>

#define MITHENZIP_PIDL_MAGIC    0x50495A4Du
#define MITHENZIP_PIDL_FLAG_DIR 0x00000001u

struct CMithenZipPidlItem
{
  USHORT cb;
  DWORD magic;
  DWORD flags;
  WCHAR name[1];
};

UINT MithenZip_PidlItemSize(LPCWSTR name);
LPITEMIDLIST MithenZip_PidlCreate(LPCWSTR name,bool isDir);
LPITEMIDLIST MithenZip_PidlCopy(LPCITEMIDLIST pidl);
UINT MithenZip_PidlLength(LPCITEMIDLIST pidl);
LPITEMIDLIST MithenZip_PidlConcat(LPCITEMIDLIST parent,LPCITEMIDLIST child);
bool MithenZip_PidlIsOurs(LPCITEMIDLIST pidl);
bool MithenZip_PidlIsDir(LPCITEMIDLIST pidl);
LPCWSTR MithenZip_PidlName(LPCITEMIDLIST pidl);
int MithenZip_PidlCompare(LPCITEMIDLIST pidl1,LPCITEMIDLIST pidl2);
PCUITEMID_CHILD MithenZip_PidlFindLastOurs(LPCITEMIDLIST pidl);

#endif
