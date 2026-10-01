#!/bin/sh
set -e
cd build
../sdk/tools/premake/premake5 gmake
cd ..
make config=release_x64 "$@"