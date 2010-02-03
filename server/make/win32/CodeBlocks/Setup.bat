rem ====================================
rem Unpack all files.
rem ====================================

rem cURL
gunzip -c curl.tar.gz > curl.tar
tar xvf curl.tar

gunzip -c pthread.tar.gz > pthread.tar
tar xvf pthread.tar

move *.dll ..\..\..\

rem ====================================
rem Clean up.
rem ====================================
del *.tar
