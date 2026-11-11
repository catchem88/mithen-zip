# MithenZip Release Notes

## v1.1.0 - 2026-11-11

### Highlights
- **Explorer stays responsive.** Adding to and extracting from large archives now runs in the background. The progress dialog keeps updating and File Explorer no longer freezes while work is in progress.
- **Clear conflict handling.** When extracting over an existing file, choose **Replace**, **Replace All**, **Keep Both**, **Skip** or **Cancel**. Replace is focused by default, so Enter replaces. Keep Both writes the new file as `name (1).ext` (Windows naming) and leaves the existing file untouched.
- **Archive engine updated to 7-Zip 26.06** (includes 26.05): security fixes, improved Z and Mach-O unpacking, and better support for split files created by the Linux `split` utility.
- **MithenZip logo on every dialog.** The Add, Extract, Progress, Password and Overwrite windows now show the MithenZip icon in the title bar instead of the generic application icon.

### Upgrade note
After installing or updating, restart File Explorer (the installer offers this) so Explorer loads the new shell extension. A running `explorer.exe` keeps the previous DLL loaded until it restarts, even though the new file is already on disk.

### Unchanged
- Browsing archives as folders, multi-part/split archives, password-protected archives, drag & drop, copy/paste, rename and delete inside archives.
