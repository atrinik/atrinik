rem ====================================
rem Unpack all files.
rem ====================================

rem SDL libraries
gunzip -c sdl_lib.tar.gz > sdl_lib.tar
tar xvf sdl_lib.tar

rem SDL includes
gunzip -c sdl_inc.tar.gz > sdl_inc.tar
tar xvf sdl_inc.tar

rem SDL dlls
gunzip -c sdl_dll.tar.gz > sdl_dll.tar
tar xvf sdl_dll.tar

rem cURL
gunzip -c curl.tar.gz > curl.tar
tar xvf curl.tar

rem ====================================
rem Copy the dll's to main directory.
rem ====================================
move sdl_dll\*.dll ..\..\..\
rmdir sdl_dll

rem ====================================
rem Clean up.
rem ====================================
del *.tar
