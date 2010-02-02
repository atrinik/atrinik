rem ====================================
rem Unpack all files.
rem ====================================

rem SDL libraries
..\tools\gunzip -c sdl_lib.tar.gz > sdl_lib.tar
..\tools\tar xvf sdl_lib.tar

rem SDL includes
..\tools\gunzip -c sdl_inc.tar.gz > sdl_inc.tar
..\tools\tar xvf sdl_inc.tar

rem SDL dlls
..\tools\gunzip -c sdl_dll.tar.gz > sdl_dll.tar
..\tools\tar xvf sdl_dll.tar

rem ====================================
rem Copy the dlls to main directory.
rem ====================================
move sdl_dll\*.dll ..\..\..\
rmdir sdl_dll

rem ====================================
rem Clean up.
rem ====================================
del *.tar
