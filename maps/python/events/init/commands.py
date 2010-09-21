## @file
## This script initialized the various Python-powered commands at
## startup time.

from Atrinik import RegisterCommand

RegisterCommand("guild", "/python/commands/guild.py", 0)
RegisterCommand("guildmembers", "/python/commands/guildmembers.py", 0)
RegisterCommand("roll", "/python/commands/roll.py", 0)
#RegisterCommand("pirate_say", "/python/commands/pirate_say.py", 1)
#RegisterCommand("pirate_shout", "/python/commands/pirate_shout.py", 1)
