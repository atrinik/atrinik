rem Run server script.

if not exist "lib" (
	echo Creating lib directory...
	md lib
)

if not exist "data" (
	echo Creating data directory...
	xcopy /s/e install_data data\
)

copy ..\arch\*.* lib\*.*
del /q data\tmp\*

if exist "server-custom.cfg" (
	set CLI_CONFIG=--config=server-custom.cfg
)

atrinik_server.exe %CLI_CONFIG% --logfile=logfile.log
