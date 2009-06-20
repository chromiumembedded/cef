@echo off
..\..\third_party\python_24\python.exe translator.py --cpp-header ..\include\cef.h --capi-header ..\include\cef_capi.h --cpptoc-dir ..\libcef_dll\cpptoc --ctocpp-dir ..\libcef_dll\ctocpp
pause