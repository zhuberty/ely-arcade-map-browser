@echo off
cd build
..\sdk\tools\premake\premake5.exe gmake || exit /b 1
cd ..
mingw32-make config=release_x64 %*