rem Atrinik Install Script
rem This is the win32 installer.
rem It will generate the data directory with all needed files
rem and generate the lib folder(if not exits) by copying from default arch
rem (this weak base installer can't handle different /arch directories)

cd ..\..
md lib
copy ..\arch\*.* lib\*.*
md data
md data\tmp
md data\log
md data\unique-items
copy install\*. data\*.
