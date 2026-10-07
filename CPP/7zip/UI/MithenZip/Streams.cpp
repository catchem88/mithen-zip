// Streams.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "Streams.h"

bool CMithenZipFileInStream::Open(LPCWSTR path)
{
  Close();
  _h = ::CreateFileW(path,GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  return _h != INVALID_HANDLE_VALUE;
}

void CMithenZipFileInStream::Close()
{
  if(_h != INVALID_HANDLE_VALUE) {
    ::CloseHandle(_h);
    _h = INVALID_HANDLE_VALUE;
  }
}

Z7_COM7F_IMF(CMithenZipFileInStream::Read(void *data,UInt32 size,UInt32 *processedSize))
{
  if(processedSize) {
    *processedSize = 0;
  }
  if(_h == INVALID_HANDLE_VALUE) {
    return E_FAIL;
  }
  DWORD rd = 0;
  if(!::ReadFile(_h,data,size,&rd,NULL)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }
  if(processedSize) {
    *processedSize = rd;
  }
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipFileInStream::Seek(Int64 offset,UInt32 seekOrigin,UInt64 *newPosition))
{
  if(_h == INVALID_HANDLE_VALUE) {
    return E_FAIL;
  }
  LARGE_INTEGER distance;
  distance.QuadPart = offset;
  LARGE_INTEGER pos;
  if(!::SetFilePointerEx(_h,distance,&pos,seekOrigin)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }
  if(newPosition) {
    *newPosition = (UInt64)pos.QuadPart;
  }
  return S_OK;
}

bool CMithenZipFileOutStream::Create(LPCWSTR path)
{
  Close();
  _h = ::CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  return _h != INVALID_HANDLE_VALUE;
}

void CMithenZipFileOutStream::Close()
{
  if(_h != INVALID_HANDLE_VALUE) {
    ::CloseHandle(_h);
    _h = INVALID_HANDLE_VALUE;
  }
}

bool CMithenZipFileOutStream::SetMTime(const FILETIME &time)
{
  if(_h == INVALID_HANDLE_VALUE) {
    return false;
  }
  return ::SetFileTime(_h,NULL,NULL,&time) != FALSE;
}

Z7_COM7F_IMF(CMithenZipFileOutStream::Write(const void *data,UInt32 size,UInt32 *processedSize))
{
  if(processedSize) {
    *processedSize = 0;
  }
  if(_h == INVALID_HANDLE_VALUE) {
    return E_FAIL;
  }
  DWORD written = 0;
  if(!::WriteFile(_h,data,size,&written,NULL)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }
  if(processedSize) {
    *processedSize = written;
  }
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipFileOutStream::Seek(Int64 offset,UInt32 seekOrigin,UInt64 *newPosition))
{
  if(_h == INVALID_HANDLE_VALUE) {
    return E_FAIL;
  }
  LARGE_INTEGER distance;
  distance.QuadPart = offset;
  LARGE_INTEGER pos;
  if(!::SetFilePointerEx(_h,distance,&pos,seekOrigin)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }
  if(newPosition) {
    *newPosition = (UInt64)pos.QuadPart;
  }
  return S_OK;
}

Z7_COM7F_IMF(CMithenZipFileOutStream::SetSize(UInt64 newSize))
{
  if(_h == INVALID_HANDLE_VALUE) {
    return E_FAIL;
  }
  LARGE_INTEGER distance;
  distance.QuadPart = (LONGLONG)newSize;
  if(!::SetFilePointerEx(_h,distance,NULL,FILE_BEGIN)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }
  if(!::SetEndOfFile(_h)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }
  return S_OK;
}
