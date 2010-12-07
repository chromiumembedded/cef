#!/bin/sh
python translator.py --cpp-header ../include/cef.h --capi-header ../include/cef_capi.h --cpptoc-dir ../libcef_dll/cpptoc --ctocpp-dir ../libcef_dll/ctocpp
