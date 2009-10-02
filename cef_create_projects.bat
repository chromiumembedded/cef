@echo off
ECHO Patching build configuration files...
CALL tools\patch_build.bat
ECHO Generating project files...
call :SET_ENV %CD%
:GEN_PROJ
CALL ..\tools\gyp\gyp.bat cef.gyp -I ..\build\common.gypi -I cef.gypi
GOTO :END
:SET_ENV
SET CEF_DIRECTORY=%~n1
GOTO :GEN_PROJ
:END