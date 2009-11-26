rem ====================================
rem Unpack all files.
rem ====================================

rem cURL
gunzip -c curl.tar.gz > curl.tar
tar xvf curl.tar

rem ====================================
rem Clean up.
rem ====================================
del *.tar
