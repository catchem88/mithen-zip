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
//Shared by the context menu and MithenZip.exe.
HRESULT MithenZip_ExtractFilesInteractive(HWND owner,const std::wstring &archivePath);

//Prompt for an archive password. Returns true when a password was entered.
bool MithenZip_ShowPasswordDialog(HWND owner,std::wstring &password);
HWND MithenZip_PromptOwner();

//Modeless progress dialog implementing CMithenZipProgress. All UI work happens on
//the thread that created it; other threads only post messages.
class CMithenZipProgressDialog Z7_final: public CMithenZipProgress
{
  HWND _hwnd;
  DWORD _ownerThread;
  CRITICAL_SECTION _cs;
  UInt64 _total;
  UInt64 _completed;
  LONG _cancelled;

  Z7_CLASS_NO_COPY(CMithenZipProgressDialog)

  void UpdateBar();

public:
  CMithenZipProgressDialog();
  ~CMithenZipProgressDialog();

  bool Create(HWND owner,const std::wstring &title);
  void Destroy();
  void PumpMessages();

  void ProgressSetTotal(UInt64 total);
  void ProgressSetCompleted(UInt64 completed);
  void ProgressSetText(const std::wstring &text);
  bool ProgressIsCancelled();
  void ProgressPump();
  HWND ProgressOwner();

  static INT_PTR CALLBACK DialogProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam);
};

#endif
