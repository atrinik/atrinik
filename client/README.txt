Welcome to the Atrinik client
----------------------------------

This client comes in different packages for different operating
systems.

So called "binary packages" usually come with an installer.
If you have run an installer before you read this you will see
an executable file in the same folder you found this README.txt
file. Your system should also be configured to run the client
without problems. 

Now some tips for the packages and OS.

Playing with Laptops
--------------------
The base installation assumes you have a num-pad keyboard.
The num-pad keys are used to move. With a laptop, usually
there are no num-pad keys. However, the movement keys are
actually bound to the num-pad keys with macros. It is easy to
change the macros so that you can bind movement to any key you
want.

To change:
Start the client and connect to the server. After you have
logged in press F11 for the keybind menu!

These macros are bound the the num-pad:
?M_CONSOLE       - opens the text console (enter on num-pad)
?M_NORTH         - move north ('9' on num-pad)
?M_NORTHEAST     - move northeast ('6' on num-pad)
?M_EAST          - move east ('3' on num-pad)
?M_SOUTHEAST     - move southeast ('2' on num-pad)
?M_SOUTH         - move south ('1' on num-pad)
?M_SOUTHWEST     - move southwest ('4' on num-pad)
?M_WEST          - move west ('7' on num-pad)
?M_NORTHWEST     - move northwest ('8' on num-pad)
?M_STAY          - stay or target self ('5' on num-pad)

To change a key, navigate to the macro in the keybind menu with
the cursor keys, and press return to edit it. Then when it asks
you to edit the macro, press return again because you don't want
to change the macro command itself. You will then be prompted to
press a key to assign the macro to. Press the key you want to
bind to the macro and it should be done. Do this for all the
movement keys, and then press 'D' for done. This will save the
newly bound keys.

You can bind any key to any command apart from the internal menu
keys and the F-keys as they are used for the quickslots. This
will allow you to customise the client as you wish.

MS Windows (2000, XP, Vista, 7, ...)
------------------------
You should have downloaded and ran the installer containing
binary files. All you need to do is double click on Atrinik.exe.
The client should work out of the box.

Note that Windows installations by default hide known file
extensions by default. In that case, the Atrinik.exe file will
simply appear as Atrinik. If that is the case, just run the
Atrinik file as it is the same file.

Source packages should ONLY be needed for developers. If you
have a simple .zip file and no installer and you see no
Atrinik.exe and there is a folder called /make in your /client
folder, then you have the source package instead of the binary
package. Just go back to the download page and get the binary
package instead.

Linux (and other *nix)
----------------------
For Ubuntu users, there is a binary package that will work out
of the box. If you have installed this package, you can simply
navigate to Applications -> Games and click on Atrinik Client to
start the client.

For other distributions of Linux, you will have to get the
source package and compile.

Source Packages
---------------
Downloading the source package requires compiling (of course).
In the /client folder, you will see a subfolder called /make.

When you navigate to this folder, you will see more subfolders
for specific operating systems. Go in the one relevant to your
operating system. Then navigate into the folder for the
compiler/IDE you will be using and open the README file in
there. This file will give you intructions on how to compile and
install for your operating system.

Note, that for some operating systems, you may be required to
first install other libraries before you can compile.
