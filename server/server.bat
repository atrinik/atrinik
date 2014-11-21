rem Run server script.

if not exist "lib" (
	echo Creating lib directory...
	md lib
)

if not exist "data" (
	echo Creating data directory...
	xcopy /s/e install_data data\
	md data\tmp
)

copy ..\arch\*.* lib\*.*

start /B python ./tools/http_server.py
atrinik-server.exe --logfile=logfile.log
