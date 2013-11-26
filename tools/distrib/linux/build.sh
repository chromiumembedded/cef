#!/bin/bash
if [ -z "$1" ]; then
  echo "ERROR: Please specify a build target: Debug or Release"
else
  make -j8 cefclient cefsimple BUILDTYPE=$1
  if [ $? -eq 0 ]; then
    echo "Giving SUID permissions to chrome-sandbox..."
    echo "(using sudo so you may be asked for your password)"
    sudo -- chown root:root "out/$1/chrome-sandbox" &&
    sudo -- chmod 4755 "out/$1/chrome-sandbox"
  fi
fi
