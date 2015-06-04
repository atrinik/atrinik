## @file
## Handles the Atrinik module print method.

import Atrinik, Markup

# Log the message to the server log.
# noinspection PyUnresolvedReferences
for line in print_msg.split("\n"):
    if line:
        Atrinik.Logger("DEBUG", line)

# Escape the markup in the message, and print it out to all online DMs.
# noinspection PyUnresolvedReferences
print_msg = Markup.markup_escape(print_msg)
player = Atrinik.GetFirst("player")

while player:
    if "[DEV]" in player.cmd_permissions or "[OP]" in player.cmd_permissions:
        player.DrawInfo(print_msg, Atrinik.COLOR_ORANGE)

    player = player.next
