## @file
## This script initialized the various Python-powered commands at
## startup time.

from Atrinik import *

RegisterCommand("guild", "/python/commands/guild.py", 1)
RegisterCommand("guildmembers", "/python/commands/guildmembers.py", 1)
RegisterCommand("roll", "/python/commands/roll.py", 1)
RegisterCommand("stime", "/python/commands/stime.py", 1)
RegisterCommand("console", "/python/commands/console.py", 1)
#RegisterCommand("pirate_say", "/python/commands/pirate_say.py", 1)
#RegisterCommand("pirate_shout", "/python/commands/pirate_shout.py", 1)
