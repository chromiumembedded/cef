@echo off
:: Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
:: reserved. Use of this source code is governed by a BSD-style license
:: that can be found in the LICENSE file.

:: Set up the environment for use with MSVS tools and then execute whatever
:: was specified on the command-line.

set RC=

:: Support !! syntax for delayed variable expansion.
setlocal enabledelayedexpansion

:: Require that platform is passed as the first argument.
if "%1" == "win32" (
  set vcvarsbat=vcvars32.bat
) else if "%1" == "win64" (
  set vcvarsbat=vcvars64.bat
) else if "%1" == "winarm64" (
  set vcvarsbat=vcvarsamd64_arm64.bat
) else (
  echo ERROR: Please specify a target platform: win32, win64 or winarm64
  set ERRORLEVEL=1
  goto end
)

:: Check if vcvars is already provided via the environment.
set vcvars="%CEF_VCVARS%"
if %vcvars% == "none" goto found_vcvars
if exist %vcvars% goto found_vcvars

:: Search for the default VS installation path.
for %%x in (2022) do (
  for %%y in ("%PROGRAMFILES%" "%PROGRAMFILES(X86)%") do (
    for %%z in (Professional Enterprise Community BuildTools) do (
      set vcvars="%%~y\Microsoft Visual Studio\%%x\%%z\VC\Auxiliary\Build\%vcvarsbat%"
      if exist !vcvars! goto found_vcvars
    )
  )
)

echo ERROR: Failed to find vcvars
set ERRORLEVEL=1
goto end

:found_vcvars
echo vcvars:
echo %vcvars%

if not %vcvars% == "none" (
  :: Set this variable to keep VS2017 < 15.5 from changing the current working directory.
  set "VSCMD_START_DIR=%CD%"
  call %vcvars%
)

echo PATH:
echo %PATH%

:: Remove the first argument and execute the command.
for /f "tokens=1,* delims= " %%a in ("%*") do set ALL_BUT_FIRST=%%b
echo command:
echo %ALL_BUT_FIRST%
%ALL_BUT_FIRST%

:end
endlocal & set RC=%ERRORLEVEL%
goto omega

:returncode
exit /B %RC%

:omega
call :returncode %RC%
