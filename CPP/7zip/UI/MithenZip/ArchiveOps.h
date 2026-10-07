// ArchiveOps.h

#ifndef ZIP7_INC_MITHENZIP_ARCHIVEOPS_H
#define ZIP7_INC_MITHENZIP_ARCHIVEOPS_H

#include "../../../Common/MyWindows.h"

#include <string>
#include <vector>

enum CMithenZipUpdateMode
{
  kMithenZipUpdate_Add = 0,
  kMithenZipUpdate_Update,
  kMithenZipUpdate_Fresh,
  kMithenZipUpdate_Sync
};

enum CMithenZipPathModeAdd
{
  kMithenZipPathAdd_Relative = 0,
  kMithenZipPathAdd_Full,
  kMithenZipPathAdd_Absolute
};

enum CMithenZipPathModeExtract
{
  kMithenZipPathExtract_Full = 0,
  kMithenZipPathExtract_No,
  kMithenZipPathExtract_Absolute
};

enum CMithenZipOverwriteMode
{
  kMithenZipOverwrite_Ask = 0,
  kMithenZipOverwrite_Overwrite,
  kMithenZipOverwrite_Skip,
  kMithenZipOverwrite_Rename,
  kMithenZipOverwrite_RenameExisting
};

struct CMithenZipCompressOptions
{
  std::wstring FormatName;      // "7z","zip","tar","gzip","bzip2","xz","wim"
  int Level;                    // 0..9
  std::wstring Method;          // empty = auto
  UInt64 Dictionary;            // 0 = auto
  UInt32 WordSize;              // 0 = auto
  UInt64 SolidBlock;            // 0 = auto
  int NumThreads;               // 0 = auto
  int UpdateMode;               // CMithenZipUpdateMode
  int PathMode;                 // CMithenZipPathModeAdd
  bool Sfx;
  bool CompressShared;
  bool DeleteAfter;
  std::wstring Password;
  std::wstring EncryptionMethod; // e.g. "AES256"
  bool EncryptFileNames;
  std::wstring ExtraParameters;  // advanced "-m" overrides (informational)

  CMithenZipCompressOptions()
    : Level(5),Dictionary(0),WordSize(0),SolidBlock(0),NumThreads(0),
      UpdateMode(kMithenZipUpdate_Add),PathMode(kMithenZipPathAdd_Relative),
      Sfx(false),CompressShared(false),DeleteAfter(false),EncryptFileNames(false)
    {}
};

struct CMithenZipExtractOptions
{
  std::wstring OutputDir;
  int PathMode;        // CMithenZipPathModeExtract
  int OverwriteMode;   // CMithenZipOverwriteMode
  bool ElimDup;
  bool NtSecurity;
  std::wstring Password;

  CMithenZipExtractOptions()
    : PathMode(kMithenZipPathExtract_Full),OverwriteMode(kMithenZipOverwrite_Ask),
      ElimDup(true),NtSecurity(false)
    {}
};

//Progress sink used by the operations (implemented by the progress dialog).
class CMithenZipProgress
{
public:
  virtual ~CMithenZipProgress() {}
  virtual void ProgressSetTotal(UInt64 total) = 0;
  virtual void ProgressSetCompleted(UInt64 completed) = 0;
  virtual void ProgressSetText(const std::wstring &text) = 0;
  virtual bool ProgressIsCancelled() = 0;
  virtual void ProgressPump() = 0;
  virtual HWND ProgressOwner() = 0;
};

//Create a new archive (Add) or update an existing one, from the given input paths.
HRESULT MithenZip_Compress(CMithenZipProgress *progress,
    const std::vector<std::wstring> &inputPaths,
    const std::wstring &archivePath,
    const CMithenZipCompressOptions &options);

//Extract every item of one archive.
HRESULT MithenZip_Extract(CMithenZipProgress *progress,
    const std::wstring &archivePath,
    const CMithenZipExtractOptions &options);

//Default archive base name for a selection (used for "Add to <name>.zip/.7z").
std::wstring MithenZip_SelectionBaseName(const std::vector<std::wstring> &inputPaths);

//Path helpers shared with the context menu / dialogs.
std::wstring MithenZip_ParentDir(const std::wstring &path);
std::wstring MithenZip_FileName(const std::wstring &path);
std::wstring MithenZip_RemoveExtension(const std::wstring &name);

#endif
