@ECHO OFF

for %%f in (..\ui\*.ui) do (
	start "" /wait cmd /C "pyuic5 ..\ui\%%~nf.ui -o ..\ui\%%~nf.py"
)

python ..\map_checker.py