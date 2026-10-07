<p align="center">
  <img src="img/mithen-zip.png" alt="MithenZip logo" width="180">
</p>

<h1 align="center">MithenZip</h1>

<div align="center">
MithenZip complements (rather than replace) Windows File Explorer's archive capability. Extract and create archives from the folder you are already in, with no separate application window. A Windows focused fork of <a href="https://www.7-zip.org/">Igor Pavlov's 7-Zip</a>.
</div>

## Features
* Archive commands grouped in one **Archive** submenu on right-click: `Add to archive...`, `Add to "name.zip"`, `Add to "name.7z"`, `Open archive`, `Extract files...`, `Extract here`.
* Compression with the full option set: format (zip, 7z, tar, gzip, bzip2, xz, wim), level, method, dictionary, word size, solid block size, CPU threads, update mode, path mode, plus encryption (password, re-enter, show password, encryption method, encrypt file names).
* Extraction options: destination, overwrite mode (ask / overwrite / skip / rename), eliminate duplication of the root folder, and password entry with show-password.
* Password protected archives are detected up front: opening one asks for the password and extracts it. A cancelled or wrong password opens nothing, and a failed extraction never leaves a corrupt file behind.
* Reads every format 7-Zip supports (7z, zip, rar, tar, gzip, bzip2, xz, cab, iso, wim and more), including multi-volume and split archives.

## Notes
* Set "MithenZip" / "MithenZip.exe" as your default application for 7z, zip, rar, etc. To ensure smooth operation on passworded archives.

## Part of MithenApps
* No telemetry
* No changing language after installation (lighter)
* No lingering background service. Closed when it's closed.
* No tracking of what "recent" files you opened. (lighter, privacy reasons)
* No update checking (use it as a tool, update it when you find issues only)
* Uninstalls cleanly, no leftovers
* Prioritizing user-ergonomics

## Credits
* [Igor Pavlov](https://www.7-zip.org/) - author of 7-Zip, which MithenZip is derived from.
* The 7-Zip contributors whose formats and codecs MithenZip builds on.