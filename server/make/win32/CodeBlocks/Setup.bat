rem ====================================
rem Unpack all files.
rem ====================================

rem cURL
gunzip -c curl.tar.gz > curl.tar
tar xvf curl.tar

rem SQLite3
gunzip -c sqlite3.tar.gz > sqlite3.tar
tar xvf sqlite3.tar

rem ====================================
rem Clean up.
rem ====================================
del *.tar
