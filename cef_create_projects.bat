@echo off
CALL tools\make_version_header.bat
..\third_party\python_26\python.exe tools\gclient_hook.py
