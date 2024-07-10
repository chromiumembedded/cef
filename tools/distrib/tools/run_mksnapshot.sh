#!/bin/bash
# Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file.

if [ "$1" != "Debug" ] && [ "$1" != "Release" ]; then
  echo 'Usage: run_mksnapshot.sh [Debug|Release] /path/to/snapshot.js'
  exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )"
BIN_DIR=$SCRIPT_DIR/$1

if [ ! -d "$BIN_DIR" ]; then
  echo "$BIN_DIR directory not found."
  exit 1
fi

CMD_FILE=mksnapshot_cmd.txt

if [ ! -f "$BIN_DIR/$CMD_FILE" ]; then
  echo "$BIN_DIR/$CMD_FILE file not found."
  exit 1
fi

pushd "$BIN_DIR" > /dev/null

# Read $CMD_FILE into an argument array.
IFS=' ' read -r -a args < $CMD_FILE

if [ "$(uname)" == "Darwin" ]; then
  # Execution of tools binaries downloaded on MacOS may be blocked
  # by Apple Security settings with a message like "<binary> can't
  # be opened because Apple cannot check it for malicious software."
  # Remove that block here.
  xattr -c ./mksnapshot
  xattr -c ./v8_context_snapshot_generator
fi

# Generate snapshot_blob.bin.
echo 'Running mksnapshot...'
./mksnapshot "${args[@]}" "${@:2}"

# Determine the architecture suffix, if any.
suffix=''
if [ "$(uname)" == "Darwin" ]; then
  value='--target_arch=arm64'
  if [[ " ${args[*]} " =~ [[:space:]]${value}[[:space:]] ]]; then
    suffix='.arm64'
  else
    suffix='.x86_64'
  fi
fi

OUT_FILE=v8_context_snapshot${suffix}.bin

# Generate v8_context_snapshot.bin.
echo 'Running v8_context_snapshot_generator...'
./v8_context_snapshot_generator --output_file=$OUT_FILE

popd > /dev/null

if [ -f "$BIN_DIR/$OUT_FILE" ]; then
  echo "Success! Created $BIN_DIR/$OUT_FILE"
else
  echo "Failed"
  exit 1
fi
