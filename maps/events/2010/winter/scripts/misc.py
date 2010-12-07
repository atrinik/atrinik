## @file
## Multi-purpose script used in the 2010 Winter Event.

from Atrinik import *

activator = WhoIsActivator()
event_num = GetEventNumber()
enabled = False

# Handle map event objects.
if WhatIsEvent().type == TYPE_MAP_EVENT_OBJ:
	# Apply event.
	if event_num == MEVENT_APPLY:
		other = WhoIsOther()

		# Do not allow players to apply lights if they haven't killed Hierro.
		if activator.type == TYPE_PLAYER and other.type == TYPE_LIGHT_APPLY and not activator.GetQuestObject("Hierro's Ring"):
			SetReturnValue(2)
			activator.Write("Something seems to prevent you from using the {}...".format(other.GetName()), COLOR_WHITE)
	else:
		# Same level as the activator.
		activator.map.difficulty = activator.level
# Normal apply event.
elif event_num == EVENT_APPLY:
	me = WhoAmI()
	quest = activator.GetQuestObject("Hierro's Ring")

	# Going down.
	if me.name == "stairs going down":
		# Event is not active or the quest has been completed, do not
		# allow going down.
		if not enabled or quest:
			activator.Write("You go down the stairs, but after a couple steps find your passage blocked by rocks. As there doesn't seem to be any way to the cave, you return.", COLOR_WHITE)
			SetReturnValue(1)
		else:
			done = 0

			# Find applyable lights in player's inventory and extinguish
			# them all.
			for obj in activator.FindObject(mode = INVENTORY_CONTAINERS, type = TYPE_LIGHT_APPLY, multiple = True):
				# Is the light actually lit?
				if obj.glow_radius:
					done += 1
					# Extinguish it.
					obj.Apply(obj, APPLY_TOGGLE)

					# If it was not applied, only lit, it will be applied
					# now, so unapply it.
					if obj.f_applied:
						obj.Apply(obj, APPLY_TOGGLE)

			if done:
				activator.Write("Some strange power has extinguished the light{} you are carrying...".format(done > 1 and "s" or ""), COLOR_YELLOW)
	# Completed the quest?
	elif quest and me.name == "stairs going up":
		activator.Write("As you go up the stairs to the surface, there is a great rumble, and rocks fall from the ceiling, blocking the passage!\nIt seems you won't be able to go back...", COLOR_WHITE)
