@echo off
call python3.bat %~dp0\translator.py --root-dir %~dp0\.. %*
pause