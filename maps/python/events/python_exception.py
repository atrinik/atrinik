## @file
## Exception handling.
##
## This is basically an improved version of Python C API exception
## printing function; this version logs the message to the active server
## log file pointer (whether it's an actual file, stdout or stderr), as
## well as to all the online DMs.

import traceback

import Atrinik
import Markup

# Construct the exception string.
# noinspection PyUnresolvedReferences
exception = "".join(traceback.format_exception(exc_type, exc_value, exc_traceback))

# Log the exception to the server log.
for line in exception.split("\n"):
    if line:
        Atrinik.Logger("BUG", line)

# Escape the markup in the exception message, and print it out to all online DMs.
exception = Markup.markup_escape(exception)
player = Atrinik.GetFirst("player")

while player:
    if "[DEV]" in player.cmd_permissions or "[OP]" in player.cmd_permissions:
        player.DrawInfo(exception, Atrinik.COLOR_RED)

    player = player.next
