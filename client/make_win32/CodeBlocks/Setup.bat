rem Unpack all files.

..\tools\gunzip -c share.tar.gz > share.tar
..\tools\tar xvf share.tar

rem Copy the dlls to main directory.
copy bin\*.dll ..\..\

rem Clean up.
del *.tar
