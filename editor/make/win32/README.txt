Before you can compile the editor, you need

a.) a installed java compiler version 1.3xx or higher.
    With this compiler comes a run time version too - install this too.
b.) a make.exe file

Then you can compile in this steps

1.) change the path macros of the file Makefile. You have to change 
    HOMEPATH =../../src/
    JARPATH =../../lib/
    LIBPATH  =d:/jdk1.3.1/lib/
    JAVAPATH =d:/jdk1.3.1/bin
    to the pathes Daimonin and the java jdk is on your system installed.
2.) run the compile.bat
3.) run the makejar.bat

Now there should be a DaimoninEditor.jar in the editor/ folder.

Start the jar file like a exe. 

Done.
