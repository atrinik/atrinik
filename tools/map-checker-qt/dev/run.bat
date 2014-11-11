@ECHO OFF

for %%f in (..\ui\*.ui) do (
	start "" /wait cmd /C "pyuic5 ..\ui\%%~nf.ui -o ..\ui\%%~nf.py"
)

cd ..
map_checker_w32.bat
