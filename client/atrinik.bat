@echo off

echo Updating Atrinik installation, please wait...

rem Make sure we are running in the directory the Batch file is in.
cd "%0\.."
cd /d "%0\.."

rem Wait a few seconds to make sure the updater has finished.
timeout /NOBREAK 2

rem Make sure no Atrinik clients are running.
taskkill /f /t /im atrinik.exe >nul 2>&1

rem Store the directory containing the portable client.
set "old_dir=%~dp0"
rem Go to the patches directory.
cd /d "%AppData%\.atrinik\temp"

rem Extract all patches using tar included with supported Windows versions.
for %%f in (*.tar.gz) do (
	echo Extracting %%f
	tar -xzf "%%f"
	del /q "%%f"
)

rem Go back to the old directory.
cd /d "%old_dir%"
rem Copy over the extracted files.
xcopy /s/e/y "%AppData%"\.atrinik\temp\*.* .\
rem Remove the temporary directory.
rmdir /s/q "%AppData%"\.atrinik\temp

rem Start up the client.
start atrinik.exe %*
exit
