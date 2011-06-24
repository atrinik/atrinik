copy arch\*.* server\lib\*.*
cd server\data\tmp
del ?*.*
cd ..\..
atrinik_server.exe -log logfile.log
cd ..
