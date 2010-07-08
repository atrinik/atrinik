## @file
## Script to execute when Nyhelobo the beholder dies.

from Atrinik import *

me = WhoAmI()
attacker = WhoIsActivator()

if attacker:
	# Could be it was spell/arrow/etc.
	if attacker.owner:
		attacker = attacker.owner

	# Message and teleport them to the public version of the map.
	attacker.Write("As soon as Nyhelobo dies, a powerful blast of energy hits the whole room, and the beholder's mind controlling equipment is destroyed. Monsters pour into the room from all over, but they are no longer controlled by the nasty beholder.", COLOR_GREEN)
	attacker.TeleportTo("/shattered_islands/strakewood_island/brynknot/sewers/sewers_b_0404", 10, 4)
