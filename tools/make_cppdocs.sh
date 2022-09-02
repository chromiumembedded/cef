#!/bin/bash
# Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file.

if ! command -v doxygen &> /dev/null
then
  echo "ERROR: Please install Doxygen" 1>&2
  exit 1
fi

# Determine the absolute path to the script directory.
SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"

# Determine the top-level CEF directory.
CEF_DIR="${SCRIPT_DIR}/.."

# Environment variables inserted into the Doxyfile via `$(VAR_NAME)` syntax.
export PROJECT_NUMBER=$(python3 ${SCRIPT_DIR}/cef_version.py current)

# Generate documentation in the docs/html directory.
# Run from the top-level CEF directory so that relative paths resolve correctly.
( cd ${CEF_DIR} && doxygen Doxyfile )

# Write a docs/index.html file.
echo "<html><head><meta http-equiv=\"refresh\" content=\"0;URL='html/index.html'\"/></head></html>" > ${CEF_DIR}/docs/index.html

