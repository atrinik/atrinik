## @file
## Script for the Brynknot Lake portal in UC I.

from Atrinik import *
from Common import freearr_x, freearr_y, SIZEOFFREE1
import random

activator = WhoIsActivator()

if not activator.CheckInventory(2, "key2", "Maplevale's amulet", "of Llwyfen"):
	quest_container = activator.Controller().quest_container

	if not quest_container.ReadKey("underground_city_lake_portal"):
		quest_container.WriteKey("underground_city_lake_portal", "true")

	activator.Write("The portal bounces you away as soon as you touch it. Only ones in possession of an amulet blessed by the elven god Llwyfen can pass through.", COLOR_RED)
	d = random.randint(1, SIZEOFFREE1)
	activator.TeleportTo(activator.map.path, activator.x + freearr_x[d], activator.y + freearr_y[d])
	SetReturnValue(1)
