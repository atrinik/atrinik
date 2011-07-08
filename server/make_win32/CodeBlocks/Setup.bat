rem ====================================
rem Unpack all files.
rem ====================================

rem cURL
..\tools\gunzip -c curl.tar.gz > curl.tar
..\tools\tar xvf curl.tar

..\tools\gunzip -c pthread.tar.gz > pthread.tar
..\tools\tar xvf pthread.tar

move pthread\* ..\*

move *.dll ..\..\..\

rem ====================================
rem Clean up.
rem ====================================
del *.tar
