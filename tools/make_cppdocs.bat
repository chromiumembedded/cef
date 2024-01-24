@echo off
:: Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
:: reserved. Use of this source code is governed by a BSD-style license
:: that can be found in the LICENSE file.

set RC=

setlocal

:: Check if DOXYGEN_EXE is already provided via the environment.
if exist "%DOXYGEN_EXE%" goto found_exe
set DOXYGEN_EXE="C:\Program Files\doxygen\bin\doxygen.exe"
if not exist %DOXYGEN_EXE% (
echo ERROR: Please install Doxygen from https://doxygen.nl/ 1>&2
set ERRORLEVEL=1
goto end
)

:found_exe

:: Environment variables inserted into the Doxyfile via `$(VAR_NAME)` syntax.
for /F %%i in ('python3.bat %~dp0\cef_version.py current') do set PROJECT_NUMBER=%%i

:: Run from the top-level CEF directory so that relative paths resolve correctly.
set CURRENT_PATH="%CD%"
cd "%~dp0\.."

:: Generate documentation in the docs/html directory.
%DOXYGEN_EXE% Doxyfile

:: Write a docs/index.html file.
echo|set /p="<html><head><meta http-equiv="refresh" content="0;URL='html/index.html'"/></head></html>" > docs/index.html

cd "%CURRENT_PATH%"

:end
endlocal & set RC=%ERRORLEVEL%
goto omega

:returncode
exit /B %RC%

:omega
call :returncode %RC%