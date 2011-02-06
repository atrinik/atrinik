## @file
## Simple trigger script to show a message to the activator.
##
## Used for objects that cannot have message to show defined, such as
## containers.

from Atrinik import *

activator = WhoIsActivator()

if activator.type == Type.PLAYER:
	activator.Write(WhatIsEvent().msg, 0)
