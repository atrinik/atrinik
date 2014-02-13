rem ====================================
rem Unpack all files.
rem ====================================

rem Extract libraries
..\tools\gunzip -c libs.tar.gz > libs.tar
..\tools\tar xvf libs.tar

rem Extract SDL dlls
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
