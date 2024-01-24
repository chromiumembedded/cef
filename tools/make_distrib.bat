@echo off
python3.bat %~dp0\make_distrib.py --output-dir %~dp0\..\binary_distrib\ %*
