@echo off
setlocal ENABLEEXTENSIONS
set KEY_NAME=HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\Environment
set VALUE_NAME=Lib
for /F "usebackq tokens=3" %%A IN (`reg query "%KEY_NAME%" /v "%VALUE_NAME%" 2^>nul ^| find "%VALUE_NAME%"`) do (
  set MINGW_PATH=%%A
)

%MINGW_PATH%\..\..\bin\mingw-get.exe install mingw-developer-toolkit-bin mingw32-base-bin mingw32-binutils-bin mingw32-gcc-dev mingw32-gdb-bin mingw32-libgcc-dll mingw32-libltdl-dev mingw32-libpthreadgc-dll mingw32-libstdc++-dll mingw32-libz-dev mingw32-make-bin mingw32-pthreads-w32-dev mingw32-w32api-dev msys-flex-bin msys-libcrypt-dev msys-make-bin

pause
