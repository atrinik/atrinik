cd ..\..\src
c:\j2sdk1.4.1_01\bin\jar cf DaimoninEditor.jar cfeditor/*.class cfeditor/textedit/textarea/*.class cfeditor/textedit/scripteditor/*.class
c:\j2sdk1.4.1_01\bin\jar umf ../make/win32/mainclass.txt DaimoninEditor.jar
copy *.jar ..\*.jar
del *.jar