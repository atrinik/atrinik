import Atrinik
import string

me = Atrinik.WhoAmI()
ac = Atrinik.WhoIsActivator()
guild_tag = "Mercenary"
map1 = "/stoneglow/stoneglow_0000"
map2 = "/stoneglow/stoneglow_merc1"

Atrinik.SetReturnValue(1)

guild_force = ac.GetGuildForce()
if guild_force.slaying != guild_tag:
	ac.Write("Only members of the Mercenaries can enter here.", 2)
	ac.Write("A strong magic guardian force pushes you back.", 3)
	ac.TeleportTo(map1, 4, 13, 0)
else:
	ac.Write("You can enter.", 2)
	ac.Write("A magic guardian force moves you down the stairs.", 4)
	ac.TeleportTo(map2, 1, 6, 0)
