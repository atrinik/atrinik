To install and compile the Atrinik client you must follow some steps:

1. Install the SDL, SDL_image and SDL_mixer packages
  
   From these packages you need all .lib files, all .dll files and all .h files.

   There are 3 ways to get these files:

   a.) in the same folder this README.txt is, are 2 .zip files named "sdl_libinc.zip"
       and "sdl_dll.zip".

       In the "sdl_libinc.zip" are all needed .h and .lib files and in the
       "sdl_dll.zip" are all .dll files. You have to unpack these files and copy all
       files from "sdl_libinc.zip" to Atrinik/client/. The SDL include files from
       the file "sdl_libinc.zip" have to be in Atrinik/client/SDL. Thats all.
 
       You don't need to install any SDL packages on a win32 OS after this.

   b.) In the download area of the project page is the utils area, there is a
       SDL_packages.zip file with all source packages you need.
       Download these package and follow the installation directions inside the
       packages. If you download the source packages, you need to compile them!

       If you change the position of the .h files and/or the libraries you need
       to change the pathes inside the VC project files and/or makefiles too.

   c.) You can download the 3 packages from the SDL homepage: 
       http://www.libsdl.org/index.php

       There you will find the newest official release of the SDL packages and
       all SDL releated information you need.

       You only need this to do when you encounter problems with the SDL versions
       which comes with the win32 binary package of Atrinik. Perhaps the SDL home
       site has a fix for it.

       If this happens, don't forget to contact the Atrinik team.
       You will get there further advices.


2.  Start the VisualC 6.x project files and/or the makefile (if there is one)
      
       Update all pathes to your configuration!


3.  Compile!
       
       You should now have a .exe in the Atrinik/client/ folder called daimonin.exe.
       Start it. If you got any "can't find xxx.dll" message, you have forgotten to copy
       the .dll files inside the same folder. It should work if you copy them.
