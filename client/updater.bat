@echo off

for %%f in (*.tar.gz) do (
	echo Extracting %%f
	gunzip -c %%f > %%~nf.tar
	tar xvf %%~nf.tar
	del %%f
)
