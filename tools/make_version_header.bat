@echo off
svn info | ..\third_party\python_26\python.exe tools\make_version_header.py --header libcef_dll\version.h
