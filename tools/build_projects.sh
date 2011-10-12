#!/bin/sh

if [ -z "$1" ]; then
  echo "ERROR: Please specify a build target: Debug or Release"
else
  if [ -z "$2" ]; then
    PROJECT_NAME='cefclient'
  else
    PROJECT_NAME=$2
  fi

  xcodebuild -project ../cef.xcodeproj -configuration $1 -target $PROJECT_NAME
fi
