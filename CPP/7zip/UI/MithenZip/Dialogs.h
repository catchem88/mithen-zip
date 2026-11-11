// Dialogs.h

#ifndef ZIP7_INC_MITHENZIP_DIALOGS_H
#define ZIP7_INC_MITHENZIP_DIALOGS_H

#include "../../../Common/MyWindows.h"
#include "ArchiveOps.h"

#include <string>
#include <vector>

//Modal option dialogs. Return true when the user pressed OK.
bool MithenZip_ShowAddDialog(HWND owner,const std::vector<std::wstring> &inputs,
    std::wstring &archivePath,CMithenZipCompressOptions &options);

bool MithenZip_ShowExtractDialog(HWND owner,CMithenZipExtractOptions &options);

//Show the Extract files dialog for one archive and run the extraction.
//Shared by the context menu and MithenZip.exe. When wait is false the call
//returns as soon as the extraction has started (the context menu must not block
//explorer.exe); when it is true the call blocks until extraction finished.
HRESULT MithenZip_ExtractFilesInteractive(HWND owner,const std::wstring &archivePath,bool wait);

//Prompt for an archive password. Returns true when a password was entered.
bool MithenZip_ShowPasswordDialog(HWND owner,std::wstring &password);
HWND MithenZip_PromptOwner();

//Prompt when an extraction file already exists. Headless runs replace silently.
CMithenZipOverwriteAnswer MithenZip_ShowOverwriteDialog(HWND owner,const std::wstring &path);

//Modeless progress dialog implementing CMithenZipProgress. All UI work happens on
//the thread that created it; other threads only post messages.
class CMithenZipProgressDialog Z7_final: public CMithenZipProgress
{
  HWND _hwnd;
  DWORD _ownerThread;
  CRITICAL_SECTION _cs;
  UInt64 _total;
  UInt64 _completed;
  std::wstring _text;
  DWORD _lastPost;
  LONG _cancelled;

  Z7_CLASS_NO_COPY(CMithenZipProgressDialog)

  void UpdateBar();

public:
  CMithenZipProgressDialog();
  ~CMithenZipProgressDialog();

  bool Create(HWND owner,const std::wstring &title);
  void Destroy();
  void PumpMessages();

  //Pump this thread's message queue until doneEvent is signalled.
  void RunUntilDone(HANDLE doneEvent);

  void ProgressSetTotal(UInt64 total);
  void ProgressSetCompleted(UInt64 completed);
  void ProgressSetText(const std::wstring &text);
  bool ProgressIsCancelled();
  void ProgressPump();
  HWND ProgressOwner();

  static INT_PTR CALLBACK DialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
};

enum CMithenZipOperationKind
{
  kMithenZipOp_Compress = 0,
  kMithenZipOp_Extract
};

//Run a compress or extract operation on background threads so the calling UI
//thread (explorer.exe) stays responsive: one thread pumps the progress dialog,
//another performs the archive work. Progress is reported from the worker and from
//the 7-Zip encoder threads, so the dialog thread only ever touches its own window.
//StartOperation returns immediately; RunOperation blocks until the operation ends
//(used by the launcher process, which must not exit early).
bool MithenZip_StartOperation(HWND owner,int kind,const std::wstring &archivePath,
    const std::vector<std::wstring> &inputs,
    const CMithenZipCompressOptions *compress,
    const CMithenZipExtractOptions *extract);

HRESULT MithenZip_RunOperation(HWND owner,int kind,const std::wstring &archivePath,
    const std::vector<std::wstring> &inputs,
    const CMithenZipCompressOptions *compress,
    const CMithenZipExtractOptions *extract);

#endif
