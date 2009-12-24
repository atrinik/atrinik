## @file
## This script is used for objects like the chests and signs
## administrators can buy, so no one else but them can steal
## them from the guild.

from Atrinik import *
from Guild import Guild

## Activator object.
activator = WhoIsActivator()

## The guild we're managing.
guild = Guild(GetOptions())

if not guild.is_administrator(activator.name) and not activator.f_wiz:
	activator.Write("You can't pick up %s." % WhoAmI().name, COLOR_WHITE)
	SetReturnValue(1)
