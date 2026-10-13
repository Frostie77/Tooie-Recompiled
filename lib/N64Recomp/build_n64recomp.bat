@echo off
rem Rebuild the (locally patched) recompiler. Can be invoked from anywhere:
rem   MSYS_NO_PATHCONV=1 cmd /c lib\N64Recomp\build_n64recomp.bat
rem Exists because nested quoting through the shell tool mangles
rem `cmd /c "...vcvars64.bat" >nul 2>&1 && ninja ...`.
rem
rem Configures first when the build directory is missing: `build/` is not
rem committed, so a fresh clone has to configure before ninja has anything to
rem enter. `msvc_env_use.bat` lives at the repository root, two levels up.
setlocal
call "%~dp0..\..\msvc_env_use.bat"
if errorlevel 1 exit /b 1
if not exist "%~dp0build\CMakeCache.txt" (
    cmake -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 exit /b 1
)
ninja -C "%~dp0build" N64Recomp.exe
if errorlevel 1 exit /b 1
endlocal
exit /b 0
