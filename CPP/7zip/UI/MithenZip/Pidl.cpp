// Pidl.cpp

#include "StdAfx.h"

#include "Pidl.h"

static const USHORT kPidlItemHeaderSize =
    (USHORT)(sizeof(CMithenZipPidlItem) - sizeof(WCHAR));

UINT MithenZip_PidlItemSize(LPCWSTR name)
{
  const size_t len = name ? wcslen(name) : 0;
  size_t size = kPidlItemHeaderSize + (len + 1) * sizeof(WCHAR);
  size = (size + 3) & ~(size_t)3;
  return (UINT)size;
}

LPITEMIDLIST MithenZip_PidlCreate(LPCWSTR name,bool isDir)
{
  const UINT itemSize = MithenZip_PidlItemSize(name);
  BYTE *p = (BYTE*)CoTaskMemAlloc(itemSize + sizeof(USHORT));
  if(!p) {
    return NULL;
  }
  CMithenZipPidlItem *item = (CMithenZipPidlItem*)p;
  item->cb = (USHORT)itemSize;
  item->magic = MITHENZIP_PIDL_MAGIC;
  item->flags = isDir ? MITHENZIP_PIDL_FLAG_DIR : 0;
  const size_t len = name ? wcslen(name) : 0;
  if(len) {
    memcpy(item->name,name,len * sizeof(WCHAR));
  }
  item->name[len] = 0;
  *((USHORT*)(p + itemSize)) = 0;
  return (LPITEMIDLIST)p;
}

UINT MithenZip_PidlLength(LPCITEMIDLIST pidl)
{
  if(!pidl) {
    return 0;
  }
  const BYTE *p = (const BYTE*)pidl;
  UINT total = 0;
  for(;;) {
    const USHORT cb = *(const USHORT*)p;
    if(cb == 0) {
      total += (UINT)sizeof(USHORT);
      break;
    }
    total += cb;
    p += cb;
  }
  return total;
}

LPITEMIDLIST MithenZip_PidlCopy(LPCITEMIDLIST pidl)
{
  if(!pidl) {
    return NULL;
  }
  const UINT size = MithenZip_PidlLength(pidl);
  BYTE *p = (BYTE*)CoTaskMemAlloc(size);
  if(!p) {
    return NULL;
  }
  memcpy(p,pidl,size);
  return (LPITEMIDLIST)p;
}

LPITEMIDLIST MithenZip_PidlConcat(LPCITEMIDLIST parent,LPCITEMIDLIST child)
{
  if(!parent) {
    return MithenZip_PidlCopy(child);
  }
  if(!child) {
    return MithenZip_PidlCopy(parent);
  }
  const UINT parentLen = MithenZip_PidlLength(parent);
  const UINT childLen = MithenZip_PidlLength(child);
  const UINT head = parentLen - (UINT)sizeof(USHORT);
  BYTE *p = (BYTE*)CoTaskMemAlloc(head + childLen);
  if(!p) {
    return NULL;
  }
  memcpy(p,parent,head);
  memcpy(p + head,child,childLen);
  return (LPITEMIDLIST)p;
}

bool MithenZip_PidlIsOurs(LPCITEMIDLIST pidl)
{
  if(!pidl) {
    return false;
  }
  const CMithenZipPidlItem *item = (const CMithenZipPidlItem*)pidl;
  if(item->cb < kPidlItemHeaderSize + sizeof(WCHAR)) {
    return false;
  }
  return item->magic == MITHENZIP_PIDL_MAGIC;
}

bool MithenZip_PidlIsDir(LPCITEMIDLIST pidl)
{
  const CMithenZipPidlItem *item = (const CMithenZipPidlItem*)pidl;
  return (item->flags & MITHENZIP_PIDL_FLAG_DIR) != 0;
}

LPCWSTR MithenZip_PidlName(LPCITEMIDLIST pidl)
{
  const CMithenZipPidlItem *item = (const CMithenZipPidlItem*)pidl;
  return item->name;
}

//Find the last item in a pidl chain that belongs to us. Used when the folder is
//reached through "explorer.exe /e,::{CLSID},<archive>" instead of a file pidl.
PCUITEMID_CHILD MithenZip_PidlFindLastOurs(LPCITEMIDLIST pidl)
{
  if(!pidl) {
    return NULL;
  }
  const BYTE *p = (const BYTE*)pidl;
  const BYTE *last = NULL;
  for(;;) {
    const USHORT cb = *(const USHORT*)p;
    if(cb == 0) {
      break;
    }
    if(cb >= kPidlItemHeaderSize + sizeof(WCHAR) &&
        ((const CMithenZipPidlItem*)p)->magic == MITHENZIP_PIDL_MAGIC) {
      last = p;
    }
    p += cb;
  }
  return (PCUITEMID_CHILD)last;
}

int MithenZip_PidlCompare(LPCITEMIDLIST pidl1,LPCITEMIDLIST pidl2)
{
  const bool dir1 = MithenZip_PidlIsDir(pidl1);
  const bool dir2 = MithenZip_PidlIsDir(pidl2);
  if(dir1 != dir2) {
    if(dir1) {
      return -1;
    }
    return 1;
  }
  return lstrcmpiW(MithenZip_PidlName(pidl1),MithenZip_PidlName(pidl2));
}
