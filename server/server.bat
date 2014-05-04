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

if exist "server-custom.cfg" (
	set CLI_CONFIG=--config=server-custom.cfg
)

start /B python ./tools/http_server.py
atrinik_server.exe %CLI_CONFIG% --logfile=logfile.log
