@echo off
:: Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
:: reserved. Use of this source code is governed by a BSD-style license
:: that can be found in the LICENSE file.

set RC=

setlocal

if not "%1" == "Debug" (
  if not "%1" == "Release" (
    echo Usage: run_mksnapshot.bat [Debug^|Release] path\to\snapshot.js
    set ERRORLEVEL=1
    goto end
  )
)

set SCRIPT_DIR=%~dp0
set BIN_DIR=%SCRIPT_DIR%%~1

if not exist "%BIN_DIR%" (
  echo %BIN_DIR% directory not found
  set ERRORLEVEL=1
  goto end
)

set CMD_FILE=mksnapshot_cmd.txt

if not exist "%BIN_DIR%\%CMD_FILE%" (
  echo %BIN_DIR%\%CMD_FILE% file not found
  set ERRORLEVEL=1
  goto end
)

cd "%BIN_DIR%"

:: Read %CMD_FILE% into a local variable.
set /p CMDLINE=<%CMD_FILE%

:: Generate snapshot_blob.bin.
echo Running mksnapshot...
call mksnapshot %CMDLINE% %2

set OUT_FILE=v8_context_snapshot.bin

:: Generate v8_context_snapshot.bin.
echo Running v8_context_snapshot_generator...
call v8_context_snapshot_generator --output_file=%OUT_FILE%

if not exist "%BIN_DIR%\%OUT_FILE%" (
  echo Failed
  set ERRORLEVEL=1
  goto end
)

echo Success! Created %BIN_DIR%\%OUT_FILE%

:end
endlocal & set RC=%ERRORLEVEL%
goto omega

:returncode
exit /B %RC%

:omega
call :returncode %RC%
