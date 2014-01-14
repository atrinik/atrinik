rem ====================================
rem Unpack all files.
rem ====================================

..\tools\gunzip -c libs.tar.gz > libs.tar
..\tools\tar xvf libs.tar

rem ====================================
rem Clean up.
rem ====================================
del *.tar
