PROG = MithenZip.dll
DEF_FILE = MithenZip.def

CFLAGS = $(CFLAGS) \
  -DZ7_LANG \
  -wd4986 \
  -D_WIN32_WINNT=0x0601 \

LIBS = $(LIBS) shlwapi.lib comctl32.lib comdlg32.lib uuid.lib

CURRENT_OBJS = \
  $O\MithenZipDll.obj \
  $O\Pidl.obj \
  $O\ArchiveEngine.obj \
  $O\Streams.obj \
  $O\ArchiveOps.obj \
  $O\Dialogs.obj \
  $O\MithenZipOpen.obj \
  $O\MithenZipContextMenu.obj \
  $O\ExtractIcon.obj \
  $O\EnumIDList.obj \
  $O\ShellFolder.obj \
  $O\DataObject.obj \

!include "../../7zip.mak"

# Build.mak's rule only lists resource.rc as a prerequisite, so editing the
# included dialog script would not rebuild the compiled resources. Touch
# resource.rc when the dialog script is newer so the normal rule kicks in.
resource.rc: MithenZipDialogs.rc
	copy /b resource.rc +,, >nul
