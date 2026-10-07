@echo off
setlocal
set "ROOT=%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo ERROR: vswhere.exe not found. Install Visual Studio 2022 with the C++ desktop workload.
  exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
  echo ERROR: no Visual Studio C++ toolset found.
  exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
  echo ERROR: failed to initialize the MSVC environment.
  exit /b 1
)

echo [1/4] Building 7z.dll engine ...
pushd "%ROOT%CPP\7zip\Bundles\Format7zF"
nmake PLATFORM=x64
if errorlevel 1 goto :fail
popd

echo [2/4] Building MithenZip.dll shell extension ...
pushd "%ROOT%CPP\7zip\UI\MithenZip"
nmake PLATFORM=x64
if errorlevel 1 goto :fail
popd

echo [3/5] Building MithenZip.exe launcher ...
pushd "%ROOT%CPP\7zip\UI\MithenZip"
rc /nologo /fo "x64\MithenZipLauncher.res" MithenZipLauncher.rc
if errorlevel 1 goto :fail
cl /nologo /EHsc /MT /O2 /DUNICODE /D_UNICODE MithenZipLauncher.cpp x64\MithenZipLauncher.res /Fe:x64\MithenZip.exe ^
  /link /SUBSYSTEM:WINDOWS /MANIFEST:NO shell32.lib user32.lib advapi32.lib
if errorlevel 1 goto :fail
popd

echo [4/5] Staging installer payload ...
copy /y "%ROOT%CPP\7zip\Bundles\Format7zF\x64\7z.dll" "%ROOT%Installer\7z.dll" >nul
if errorlevel 1 goto :fail
copy /y "%ROOT%CPP\7zip\UI\MithenZip\x64\MithenZip.dll" "%ROOT%Installer\MithenZip.dll" >nul
if errorlevel 1 goto :fail
copy /y "%ROOT%CPP\7zip\UI\MithenZip\x64\MithenZip.exe" "%ROOT%Installer\MithenZip.exe" >nul

if errorlevel 1 goto :fail

echo [5/5] Building installer ...
pushd "%ROOT%Installer"
rc /nologo /foMithenZipSetup.res MithenZipSetup.rc
if errorlevel 1 goto :fail
cl /nologo /EHsc /MT /O2 /DUNICODE /D_UNICODE MithenZipSetup.cpp MithenZipSetup.res /Fe:MithenZipSetup.exe ^
  /link /SUBSYSTEM:WINDOWS /MANIFEST:NO ^
  ole32.lib oleaut32.lib shell32.lib shlwapi.lib comctl32.lib user32.lib advapi32.lib
if errorlevel 1 goto :fail
popd

if not exist "%ROOT%res" mkdir "%ROOT%res"
copy /y "%ROOT%Installer\MithenZipSetup.exe" "%ROOT%res\MithenZip-setup.exe" >nul
if errorlevel 1 goto :fail

echo.
echo Done. Installer: "%ROOT%res\MithenZip-setup.exe"
exit /b 0

:fail
echo.
echo BUILD FAILED
exit /b 1
