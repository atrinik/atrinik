## @file
## This script initialized the various Python-powered commands at
## startup time.

from Atrinik import *

COMMAND_PERMISSION = 1
COMMAND_ALLOW_MARKUP = 2

commands = [
	("guild", 1, 0),
	("guildmembers", 1, 0),
	("roll", 1, 0),
	("stime", 1, 0),
	("console", 1, COMMAND_PERMISSION | COMMAND_ALLOW_MARKUP),
	("create", 1, COMMAND_PERMISSION),
	("mapinfo", 1, COMMAND_PERMISSION),
	("rainbow", 1, COMMAND_PERMISSION),
#	("pirate_say", 1, 0),
#	("pirate_shout", 1, 0),
]

for t in commands:
	RegisterCommand(*t)
