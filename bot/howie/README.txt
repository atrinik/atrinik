Howie - A Conversational AI Bot
(http://howie.sourceforge.net/)
Copyright (C) 2003-2004 Cort Stratton (cort@andrew.cmu.edu)

ABOUT HOWIE
-----------
Howie is a conversational AI -- a computer program you can talk to.  He
is designed to be simple to install, configure and (above all) extend.

Howie is Friendly.
Currently, Howie is willing to discuss religion, sports, sex (reluctantly),
and philosophy.  He even knows a little German!

Howie is Helpful.
Howie is able to answer most questions of the form "Who/what/where/when 
is/was/are/were X?" -- though his answers are occasionally more
entertaining than accurate. He can also help find rhymes for a word, or
tell you what an acronym means. He will someday provide definitions or
synonyms for any word in the dictionary, query your address book, and even
perform a few simple remote system-administration tasks!

Howie is Connected.
You can communicate with Howie in a number of different ways: AIM, ICQ, IRC, MSN
Messenger, Jabber Chat, XML-RPC, or locally on the command-line. Future
versions will include interfaces for HTTP, Zephyr, and more!
Howie's knowledge base is made up of a wide variety of Internet resources.
If you find something that he can't do, it's usually pretty
straightforward to teach him!

SYSTEM REQUIREMENTS
-------------------
Howie has been tested on Windows XP, Red Hat Linux and NetBSD.  However,
he should work on any system that meets the following requirements:
 - Python 2.2.x or greater.
 - 64 MB available RAM (depending on the AIML set you're using)
 - Dedicated Internet connection
 - The Twisted framework (http://twistedmatrix.com/products/twisted).
   You must have Twisted installed in order to run Howie from the source.
   NOTE: The Windows binary distribution already includes Twisted.

NOTE: There is a known problem with Howie running under Windows 95, having
to do with Python's os.popen() function not working on that platform.

INSTALLATION (Windows binary distribution)
------------------------------------------
1. Run the installer (you've done that).
2. Edit the howie.ini file, found in the directory where you installed Howie.
   (You can access this file through the "Howie->Configure Howie" shortcut in
   your Start Menu).  This file lets you set up the interfaces presented to
   visitors (AIM, MSN, etc.) as well as customize Howie's behavior.  Each
   section has an "active" parameter, which indicates whether that module is
   active ("yes") or not.
3. To test Howie in local-mode (text console interface only), use the
   "Howie->Run Howie (local)" shortcut in your Start Menu.  This will let you
   chat with Howie without opening any connections to the Internet.
4. Once you're satisfied with your setup, run the "Howie->Run Howie" shortcut,
   found in your Start Menu.  This launches Howie and makes him available to
   world at large!

INSTALLATION (Source distribution)
----------------------------------
1. Unzip the archive (you've done that).
2. UNIX USERS: run the "unix-install.py" script found in the main Howie
   directory.  It will automatically configure Howie to run in a UNIX-like
   environment.
3. Edit the howie.ini file, found in the directory where you installed Howie.
   This file lets you set up the interfaces presented to visitors (AIM, MSN,
   etc.) as well as customize Howie's behavior.  Each section has an "active"
   parameter, which indicates whether that module is active ("yes") or not.
4. To test Howie locally, run "runme.py -l" in the main Howie directory.
   This lets you chat with Howie without activating any of his network
   services.
5. Once you're satisfied with your setup, run "runme.py" in the main Howie
   directory.  This launches Howie and makes him available to the world at
   large!

ACKNOWLEDGEMENTS
----------------
The Win32 installer was produced using the Nullsoft Install System
(http://www.nullsoft.com/free/nsis/).

The Win32 binary distribution is made possible thanks to py2exe
(http://starship.python.net/crew/theller/py2exe/).

Howie's conversational back-end is provided by PyAIML
(http://pyaiml.sourceforge.net), a Python implementation of Dr. Richard S.
Wallace's A.L.I.C.E. system. For more information about the A.L.I.C.E.
Foundation (including information on AIML, the Artificial Intelligence
Markup Language used to program Howie's responses), visit the A.L.I.C.E.
homepage at http://www.alicebot.org/. 

Howie's AIM and ICQ front-ends are provided by Twisted
(http://twistedmatrix.com/products/twisted).

Howie's IRC front-end was written by Jamie Turner (author of Py-TOC), and is
built on top of Joel Rosdahl's Python IRC library
(http://python-irclib.sourceforge.net/).

Howie's MSN front-end is provided by msnp, a Python MSN module by Manish
Jethani (http://msnp.sourceforge.net/). 

Howie's Jabber front-end is provided by xmpppy (http://xmpppy.sourceforge.net).

Howie is named after Howie Day (http://www.howieday.com/), for no particular
reason.  If it weren't for Amy Kalson, he would be named something truly boring
like PyBot or PyBorg.

Hooray for Python! (http://www.python.org/)