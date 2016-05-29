@echo off
:: Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
:: reserved. Use of this source code is governed by a BSD-style license
:: that can be found in the LICENSE file.

:: Set up the environment for use with MSVS tools and then execute whatever
:: was specified on the command-line.

set RC=
setlocal

:: Require that platform is passed as the first argument.
set ARGSOK=F
if "%1" == "win32" set ARGSOK=T
if "%1" == "win64" set ARGSOK=T
if "%ARGSOK%" == "F" (
  echo ERROR: Please specify a target platform: win32 or win64
  set ERRORLEVEL=1
  goto end
)

:: In case vcvars is already provided via the environment.
set vcvars="%CEF_VCVARS%"
if exist %vcvars% goto found_vcvars
if %vcvars% == "none" goto found_vcvars

if "%1" == "win64" goto check_win64

:: Hardcoded list of MSVS paths for VS2015 32-bit builds.
set vcvars="%PROGRAMFILES(X86)%\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars
set vcvars="%PROGRAMFILES%\Microsoft Visual Studio 14.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars

:check_win64
:: Hardcoded list of MSVS paths for VS2015 64-bit builds.
set vcvars="%PROGRAMFILES(X86)%\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
if exist %vcvars% goto found_vcvars
set vcvars="%PROGRAMFILES%\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
if exist %vcvars% goto found_vcvars
goto notfound_vcvars

:notfound_vcvars
echo ERROR: Failed to find vcvars
set ERRORLEVEL=1
goto end

:found_vcvars
echo vcvars:
echo %vcvars%
if not %vcvars% == "none" call %vcvars%

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
