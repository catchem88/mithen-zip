// MithenZip.h

#ifndef ZIP7_INC_MITHENZIP_H
#define ZIP7_INC_MITHENZIP_H

#include "../../../Common/MyWindows.h"

// {7B5E2C1A-9D34-4F62-A1B7-3C8E5D9F0A21} shell folder (StorageHandler)
static const GUID CLSID_MithenZipRar =
  {0x7B5E2C1A,0x9D34,0x4F62,{0xA1,0xB7,0x3C,0x8E,0x5D,0x9F,0x0A,0x21}};

// {7B5E2C1A-9D34-4F62-A1B7-3C8E5D9F0A22} context menu handler
static const GUID CLSID_MithenZipContextMenu =
  {0x7B5E2C1A,0x9D34,0x4F62,{0xA1,0xB7,0x3C,0x8E,0x5D,0x9F,0x0A,0x22}};

#define MITHENZIP_CLSID_STR      L"{7B5E2C1A-9D34-4F62-A1B7-3C8E5D9F0A21}"
#define MITHENZIP_CTX_CLSID_STR  L"{7B5E2C1A-9D34-4F62-A1B7-3C8E5D9F0A22}"
#define MITHENZIP_PROGID         L"MithenZip.Archive"
#define MITHENZIP_LEGACY_PROGID  L"MithenZip.Rar"
#define MITHENZIP_FRIENDLY_NAME  L"MithenZip Archive"
#define MITHENZIP_SHELLEXT_NAME  L"MithenZip Shell Extension"
#define MITHENZIP_DLL_NAME       L"MithenZip.dll"

//Returned when extraction fails because the password is wrong.
#define MITHENZIP_E_WRONG_PASSWORD ((HRESULT)0x80040001L)

extern HINSTANCE g_hInstance;
extern LONG g_dllRefCount;

#endif
