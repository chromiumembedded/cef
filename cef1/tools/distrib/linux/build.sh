#!/bin/bash
if [ -z "$1" ]; then
  echo "ERROR: Please specify a build target: Debug or Release"
else
  make -j8 cefclient BUILDTYPE=$1
fi


