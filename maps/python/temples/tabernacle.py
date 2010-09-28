## @file
## Script for Tabernacle temple priests.

from Atrinik import *
import Temple

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Tabernacle.
class TempleTabernacle(Temple.BaseTemple):
	name = "Tabernacle"
	desc = "I am a servant of the God of Light and King of the Angels, Tabernacle.\nIf you would like to join our Church, please touch the altar and Tabernacle will smile upon you."
	enemy_name = "Rashindel"
	enemy_desc = "Caution child, for you speak of the Fallen One.  In the days before the worlds were created by our Lord Tabernacle, the archangel Rashindel stood at his right hand.  In that day, however, Rashindel sought to claim the throne of Heaven and unseat the Mighty Tabernacle.  The Demon King was quickly defeated and banished to Hell with the angels he managed to deceive and they were transformed into the awful demons and devils which threaten the lands to this day."

def main():
	Temple.handle_temple(TempleTabernacle, me, activator, msg)

main()
