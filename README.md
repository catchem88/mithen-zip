<p align="center">
  <img src="img/mithen-zip.png" alt="MithenZip logo" width="180">
</p>

<h1 align="center">MithenZip</h1>

<div align="center">
MithenZip complements Windows File Explorer's built-in archive support (Windows already reads many archive formats) rather than replacing it. Browse, extract and create archives from the folder you are already in, with no separate application window. A Windows-focused fork of <a href="https://www.7-zip.org/">Igor Pavlov's 7-Zip</a>.
</div>

## Features
* **Archives as Explorer folders**: open `.7z`, `.rar`, `.zip`, `.tar`, `.iso`, `.wim`, `.cab`, `.gz`, `.xz` and everything else 7-Zip supports directly in File Explorer, including multi-part and split archives (`.7z.001`, `.part1.rar`, and split files created by Linux tools).
* **Archive commands in one submenu** on right-click: `Add to archive...`, `Add to "name.zip"`, `Add to "name.7z"`, plus `Open archive`, `Extract files...` and `Extract here` for archives.
* **Compression with the full option set**: format (zip, 7z, tar, gzip, bzip2, xz, wim), level, method, dictionary, word size, solid block size, CPU threads, update mode, path mode, and encryption (password, re-enter, show password, encryption method, encrypt file names).
* **Extraction options**: destination, path mode, eliminate duplication of the root folder, and overwrite behavior. When a file already exists you can choose **Replace**, **Replace All**, **Keep Both** (the new file is renamed with Windows' default `name (1).ext` convention, so the existing file stays), **Skip** or **Cancel**. Replace is the default choice, so Enter replaces.
* **Responsive by design**: adding to or extracting from a large folder runs in the background, so File Explorer stays usable and the progress bar keeps moving while the archive is written.
* **Password protected archives are detected up front**: opening one asks for the password and extracts it. A cancelled or wrong password opens nothing, and a failed extraction never leaves a corrupt file behind.
* **Drag & drop, copy/paste, rename and delete** work inside archive folders through native Explorer gestures.
* **Reads every format 7-Zip supports** (7z, zip, rar, tar, gzip, bzip2, xz, cab, iso, wim and more).

## Get Started
1. Run `res\MithenZip-setup.exe` and approve the elevation prompt. Keep **Associate all supported file types** checked to let MithenZip handle archives, and **Restart file explorer** so the shell extension loads.
2. Open any archive in File Explorer. It is shown as a folder; navigate into it as usual.
3. To create an archive, right-click one or more files or folders and use the **Archive** submenu. To extract, right-click an archive and choose **Extract here** or **Extract files...**.
4. To remove MithenZip, use **Add or remove programs** or run the installer with `/uninstall`.

No standalone window and no background service; the extension lives inside Explorer and unloads with it.

### Build from source
* Requirements: Windows 10/11 x64, Visual Studio 2022 with the C++ desktop workload (MSVC, Windows SDK, MASM).
* Run `build.cmd` from the repository root. It builds `7z.dll`, `MithenZip.dll`, the `MithenZip.exe` launcher, and packages `res\MithenZip-setup.exe`.

## Notes
* Set **MithenZip** as your default application for 7z, zip, rar, etc. for the smoothest experience (especially with password-protected archives). The installer offers to do this.
* Encrypted archives prompt with a native password dialog; passwords are never cached or written to disk.

## Screenshot
No dedicated GUI! No screenshots!

## Part of MithenApps
* No telemetry
* No changing language after installation (lighter)
* No lingering background service. Closed when it's closed.
* No tracking of what "recent" files you opened. (lighter, privacy reasons)
* No update checking (use it as a tool, update it when you find issues only)
* Uninstalls cleanly, no leftovers
* Prioritizing user-ergonomics

## Support and Sponsor the OG creator!
[Igor Pavlov](https://www.7-zip.org/)

## Credits
* [Igor Pavlov](https://www.7-zip.org/) - author of 7-Zip, which MithenZip is derived from.
* The 7-Zip contributors whose formats and codecs MithenZip builds on.
