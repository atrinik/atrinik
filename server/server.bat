@echo off
setlocal

rem Run from the packaged server directory so relative runtime paths resolve.
cd /d "%~dp0"

if not exist "lib" (
	echo The packaged server resources are missing from the lib directory.
	exit /b 1
)

if not exist "data" (
	echo Creating data directory...
	xcopy /e /i /q /y install_data data >nul
)

if not exist "data\tmp" md "data\tmp"

atrinik-server.exe %*
