======================
=== Atrinik Server ===
======================

The Atrinik Server package comes in either a binary version or a source version.
The binary package should contain the atrinik_server executable in the same
directory as this README file, and no /make or /src directories. If you have this
version of the package, you can skip the compiling stage and simply proceed to the
INSTALLATION section of this file.

In either case you will need to have Python installed. If you don't have it
installed yet, download it from http://www.python.org/ and install it.

=============
= COMPILING =
=============
If you have the source package, it should contain a /make directory. Before you can
install and run the server, you will need to compile it first.

To compile your Atrinik server and the plugins necessary to run all elements of the
game, you must install the Python scripting engine first.

On a Linux system, Python is often pre-installed. On a Windows system you usually
need to install it first.

You can download the Python script engine from the Python homepage:

http://www.python.org/

Version: Be sure your Python version is at least 2.5 or higher.
Lower versions will invoke problems with the scripts.

Once you have Python installed, you must go into the /make directory and then into
the subdirectory for the OS you are running. Finally, navigate to the directory for
the IDE/compiler you will be using, and read the README file in there. That file
will give the necessary instructions for you to compile the server for your OS and
IDE/compiler.

When the server is compiled, proceed to the INSTALLATION section of this file.

================
= INSTALLATION =
================

First of all, you will need to run the 'collect.py' script in the '../tools' directory.
Do not confuse it with the 'tools' directory in the directory with this README file!
You will need to double-click the 'collect.py' file in that directory, or run it from
the command line (the atrinikloop shell script does this automatically). This will
collect the arches, treasures, and so on.

To install the server you must go in the /install directory and run the installation
script for your OS. Navigate to the subdirectory relevant to the OS you are running.
That directory will contain the script which will install the necessary files that
the server requires to run with.

After the server is installed, you can start the atrinik_server executable in order
to run it.

Help: www.atrinik.org

