To build the Atrinik server with Visual C++, you will need to:

1. Run Setup.bat, which will unpack the libraries, dlls and header files contained
in the tar.gz file.

2. Copy the Python header files into /libs/python. They should be found in the
/include directory within the directory where you installed Python.

3. Copy the Python library across. Atrinik Server only requires pythonxx.lib, where
xx denotes the version of the Python library. If it is a 32-bit library, copy it to
/libs/python which should contain the header files you copied across. If the library
is 64-bit, copy it to /libs/python/x64 instead. The libraries should be found in the
/libs directory within the directory where you installed Python.

4. Double click on atrinik_server.sln to open the project.

5. Select build configuration. This should be Win32 if you used the 32-bit library
or x64 if you used the 64-bit library. You should compile it in Release mode unless
you used a debug Python library. The debug library will be called pythonxx_d.lib
instead of pythonxx.lib.

6. The version of the Python libraries the project file is pointing to may be
different than the ones you have installed. If that is the case, you just need to
change the name of the libraries in the project so that it correctly points to the
ones you have copied over.

7. Build solution.
