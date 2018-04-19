@echo off
call python.bat %~dp0\translator.py --root-dir %~dp0\.. %*
pause