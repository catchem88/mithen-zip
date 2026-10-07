// Streams.h

#ifndef ZIP7_INC_MITHENZIP_STREAMS_H
#define ZIP7_INC_MITHENZIP_STREAMS_H

#include "../../../Common/Common0.h"
#include "../../../Common/MyCom.h"
#include "../../IStream.h"

//Read-only IInStream over a Win32 file handle.
class CMithenZipFileInStream Z7_final : public IInStream,public CMyUnknownImp
{
  Z7_IFACES_IMP_UNK_2(ISequentialInStream,IInStream)

  HANDLE _h;

public:
  CMithenZipFileInStream(): _h(INVALID_HANDLE_VALUE) {}
  ~CMithenZipFileInStream() { Close(); }

  bool Open(LPCWSTR path);
  void Close();
};

//Write IOutStream over a Win32 file handle.
class CMithenZipFileOutStream Z7_final : public IOutStream,public CMyUnknownImp
{
  Z7_IFACES_IMP_UNK_2(ISequentialOutStream,IOutStream)

  HANDLE _h;

public:
  CMithenZipFileOutStream(): _h(INVALID_HANDLE_VALUE) {}
  ~CMithenZipFileOutStream() { Close(); }

  bool Create(LPCWSTR path);
  void Close();
  bool SetMTime(const FILETIME &time);
};

#endif
