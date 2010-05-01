To build the Atrinik client with Visual C++, you will need to:

1. Run Setup.bat or Setup_x64.bat, which will unpack the tar.gz
   files and place the required dlls in main directory.
   
   Setup.bat will unpack the 32-bit dll files, while
   Setup_x64.bat will unpack the 64-bit dll files. Do not run
   both batch files, as the dll files from one will be
   overwritten with the files from the other.

   If you are unsure on which batch file to run, just run the
   regular Setup.bat for the 32-bit installation as it will work
   in both the 32-bit and 64-bit editions of Windows. Also, the
   64-bit client is very much in testing at the moment.

2. Double click on Atrinik.sln to open the project.

3. Select build configuration. This should be Win32 if you used
   Setup.bat or x64 if you ran Setup_x64.bat.

4. Build solution.

----------------------------------------------------------------

Below are a list of dependencies and their versions for
reference. It's best to include them here so that we know when
they should be updated, and it makes it easier to find out
whether or not specific versions are compatible with each other.

SDL:       1.2.14 (with patch r4990 reverted)
           http://www.libsdl.org
           Used by Atrinik
SDL_image: 1.2.10
           http://www.libsdl.org/projects/SDL_image
           Used by Atrinik
SDL_mixer: 1.2.11
           http://www.libsdl.org/projects/SDL_mixer
           Used by Atrinik
libcurl:   7.20.1 (HTTP, FTP and FILE)
           http://curl.haxx.se
           Used by Atrinik
zlib:      1.2.5
           http://www.zlib.net
           Used by libpng and Atrinik
libpng:    1.4.1
           http://www.libpng.org
           Used by SDL_image
libogg:    1.2.0
           http://xiph.org/ogg
           Used by SDL_mixer
libvorbis: 1.3.1
           http://xiph.org/ogg
           Used by SDL_mixer
