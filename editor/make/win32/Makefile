HOMEPATH =../../src/
JARPATH =../../lib/
LIBPATH  =c:/j2sdk1.4.1_01/lib/
JAVAPATH =c:/j2sdk1.4.1_01/bin/

ALLSRC = $(wildcard $(HOMEPATH)cfeditor/*.java $(HOMEPATH)cfeditor/textedit/scripteditor/*.java $(HOMEPATH)cfeditor/textedit/textarea/*.java)
CLASSPATH = $(JARPATH);$(HOMEPATH);$(LIBPATH)tools.jar;$(JARPATH)png.jar;$(JARPATH)visualtek.jar;$(JARPATH)jdom.jar;$(JARPATH)crimson.jar;$(LIBPATH)../jre/lib/rt.jar

all:
	$(JAVAPATH)javac -classpath $(CLASSPATH) $(ALLSRC)

run:
	$(JAVAPATH)java -classpath $(CLASSPATH);$(HOMEPATH)..;class/:$(JARPATH)png.jar cfeditor.DaimoninEditor


