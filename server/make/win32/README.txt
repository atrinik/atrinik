How to Compile Atrinik Server on Windows:

Please follow the steps in the right order!
(Note: The Visual C++ project is our default one. The way to compile
       will be of course different for other compilers. Browse the make/win32
       folder for your system and read the README.txt inside for more infos.)

o First step: Before you can compile the Atrinik server source,
  you must install a python script engine on your computer.
  Python is the default script engine of the Atrinik server.
  You can find the python engine here: http://www.python.org/.
  The current Atrinik server use Python 2.x.x .
  The package comes with installer and pre compiled libs & dlls,
  you only have to run the installer. The VisualC Atrinik server settings are
  for d:\python2x . You can install the package of course on a different
  spot, but you habe then to change the pathes in the VC settings.
  Go in settings/c++ and settings/link and change the optional
  include and libs path to the new python installation path.

o 2nd step: You must compile the program.
  After unpacking the Atrinik package, run the "atrinik_server.dsw". 
  There are 3 projects in your workspace: atrinik_server (the server),
  libcross.lib and plugin_python . You need to compile all 3. Easiest
  way is to select the atrinik_server - ReleaseLog as active project,
  this will compile all others automatically too.
  Then - in visual c++ - press <F7> to compile.

  In the win32 folder of the /dev folder is a flex.exe. This is the flex
  program, used to generate loader.c from loader.l and reader.c from reader.l.
  The vc project setup will run this automatically using custom build commands.
  Don't change the position of the flex.exe or the folder tree of the projects or
  the flex run will fail! You can change the content of loader.l/reader.l in the
  vc studio like a normal c-file. The compiler will look at the depencies and generate
  the .c files when the .l files are from a newer date. Just compile normal with F7.

o Last step: If the compile was successful, you should have now a
  atrinik_server.exe inside your Atrinik/server folder and a
  python_plugin.dll inside your Atrinik/server/plugins folder
  (the vc studio will copy this files automatically after a succesfull
  compile to this folders). 

  If the files are on the right place, you are done!

  Change to the directory Atrinik/server/install/win32 and read the
  README.txt there.

If you still have troubles, go to the Atrinik home page and
search there for more help. You can also find there mailing lists
for more infos.

http://atrinik.sourceforge.net
 
Tell us exactly what version of Atrinik you've used.
If it's an old version, update first and see if that solves your problem.
