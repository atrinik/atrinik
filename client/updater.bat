@echo off

set old_dir=%CD%
cd %AppData%\.atrinik\temp

for %%f in (*.tar.gz) do (
	echo Extracting %%f
	gunzip -c %%f > %%~nf
	tar xvf %%~nf
	del /q %%f
	del /q %%~nf
)

cd %old_dir%
xcopy /s/e %AppData%\.atrinik\temp\*.* .\
del /q %AppData%\.atrinik\temp
