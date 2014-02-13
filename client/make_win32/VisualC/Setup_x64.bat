rem ====================================
rem Unpack all files.
rem ====================================

rem Extract libraries
..\tools\gunzip -c libs.tar.gz > libs.tar
..\tools\tar xvf libs.tar

rem Extract SDL dlls
..\tools\gunzip -c sdl_dll_x64.tar.gz > sdl_dll_x64.tar
..\tools\tar xvf sdl_dll_x64.tar

rem ====================================
rem Copy the dlls to main directory.
rem ====================================
move sdl_dll_x64\*.dll ..\..\..\
rmdir sdl_dll_x64

rem ====================================
rem Clean up.
rem ====================================
del *.tar
