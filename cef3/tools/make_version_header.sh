#!/bin/sh
python tools/make_version_header.py --header include/cef_version.h --cef_version VERSION --chrome_version ../chrome/VERSION --cpp_header_dir include
