@echo off
:: Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
:: reserved. Use of this source code is governed by a BSD-style license
:: that can be found in the LICENSE file.

:: Set up the environment for use with MSVS tools and then execute whatever
:: was specified on the command-line.

set RC=
setlocal

:: In case it's already provided via the environment.
set vcvars="%CEF_VCVARS%"
if exist %vcvars% goto found_vcvars

:: Hardcoded list of MSVS paths.
:: Alternatively we could 'reg query' this key:
:: HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\10.0\Setup\VS;ProductDir
set vcvars="%PROGRAMFILES(X86)%\Microsoft Visual Studio 12.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars
set vcvars="%PROGRAMFILES(X86)%\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars
set vcvars="%PROGRAMFILES(X86)%\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars
set vcvars="%PROGRAMFILES%\Microsoft Visual Studio 12.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars
set vcvars="%PROGRAMFILES%\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars
set vcvars="%PROGRAMFILES%\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars
:: VS 2008 vcvars isn't standalone, it needs this env var.
set VS90COMNTOOLS=%PROGRAMFILES(X86)%\Microsoft Visual Studio 9.0\Common7\Tools\
set vcvars="%PROGRAMFILES(X86)%\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars
set VS90COMNTOOLS=%PROGRAMFILES%\Microsoft Visual Studio 9.0\Common7\Tools\
set vcvars="%PROGRAMFILES%\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat"
if exist %vcvars% goto found_vcvars

set RC=1
echo Failed to find vcvars
goto end

:found_vcvars
echo vcvars:
echo %vcvars%
call %vcvars%

echo PATH:
echo %PATH%
%*

:end
endlocal & set RC=%ERRORLEVEL%
goto omega

:returncode
exit /B %RC%

:omega
call :returncode %RC%
