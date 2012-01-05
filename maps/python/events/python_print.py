## @file
## Handles the Atrinik module print method.

import Atrinik, Markup

# Log the message to the server log.
for line in print_msg.split("\n"):
	if line:
		Atrinik.Logger("WARNING", line)

# Escape the markup in the message, and print it out to all online DMs.
print_msg = Markup.markup_escape(print_msg)
player = Atrinik.GetFirst("player")

while player:
	if player.ob.f_wiz or "dm" in player.cmd_permissions or "resetmap" in player.cmd_permissions:
		player.ob.Write(print_msg, Atrinik.COLOR_ORANGE)

	player = player.next
