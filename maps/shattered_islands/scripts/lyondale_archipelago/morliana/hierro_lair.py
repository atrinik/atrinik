## @file
## Script for the Hierro's Lair stairs entrance.

from QuestManager import get_quest_object

def main():
	if GetTime()["season_name"] != "Season of the Blizzard" or get_quest_object(activator, "Hierro's Ring"):
		activator.Write("You go down the stairs, but after a couple steps find your passage blocked by rocks. As there doesn't seem to be any way to the cave, you return.", COLOR_YELLOW)
		SetReturnValue(1)
	else:
		extinguished = 0

		# Find applyable lights in player's inventory and extinguish
		# them all.
		for obj in activator.FindObject(mode = INVENTORY_CONTAINERS, type = Type.LIGHT_APPLY, multiple = True):
			# Is the light actually lit?
			if obj.glow_radius:
				extinguished += 1
				# Extinguish it.
				obj.Apply(obj, APPLY_TOGGLE)

				# If it was not applied, only lit, it will be applied
				# now, so unapply it.
				if obj.f_applied:
					obj.Apply(obj, APPLY_TOGGLE)

		if extinguished:
			from Language import pluralize
			activator.Write("Some strange power has extinguished the {} you are carrying...".format(pluralize("light", extinguished)), COLOR_YELLOW)

main()
