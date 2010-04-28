## @file
## This script initialized the various Python-powered commands at
## startup time.

from Atrinik import RegisterCommand

RegisterCommand("guild", "/python/commands/guild.py", 0)
RegisterCommand("guildmembers", "/python/commands/guildmembers.py", 0)
RegisterCommand("roll", "/python/commands/roll.py", 0)
