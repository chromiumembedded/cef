#!/bin/sh
DIR=$(cd "$(dirname "$0")"; pwd)
export DYLD_FALLBACK_LIBRARY_PATH="$DIR:/lib:/usr/lib"
exec "$DIR/cefclient"

