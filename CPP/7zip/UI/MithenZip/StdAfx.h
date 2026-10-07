// StdAfx.h

#if _MSC_VER >= 1800
#pragma warning(disable : 4464) // relative include path contains '..'
#endif

#include "../../../Common/Common.h"

#include <windows.h>
#include <objbase.h>
#include <objidl.h>
#include <ole2.h>
#include <propidl.h>
#include <olectl.h>
#include <shlobj.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <commctrl.h>

#include <vector>
#include <string>

#ifndef SELFREG_E_CLASS
#define SELFREG_E_CLASS E_FAIL
#endif
