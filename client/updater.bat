@echo off

for %%f in (*.tar.gz) do (
	echo Extracting %%f
	gunzip -c %%f > %%~nf
	tar xvf %%~nf
	del %%f
	del %%~nf
)

start atrinik.exe %*
