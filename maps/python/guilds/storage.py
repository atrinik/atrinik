## @file
## Handles map-wide events in guild storage maps.

from Atrinik import *
from Guild import Guild

activator = WhoIsActivator()
other = WhoIsOther()
event_num = GetEventNumber()

## Check whether the specified object is inside a player.
## @return True if the object is inside a player, False otherwise.
def is_in_player(obj):
	while obj.env:
		obj = obj.env

	return obj.type == TYPE_PLAYER

def main():
	# Don't allow skills to be used, except by administrators/DMs.
	if event_num == MEVENT_SKILL_USED:
		if guild.member_is_admin(activator.name) or activator.f_wiz:
			return

		activator.Write("You can't use skills here.", COLOR_RED)
		SetReturnValue(1)
	# Allow anyone to drop items and put them to containers, but log it.
	elif event_num == MEVENT_DROP:
		# Find objects on the floor layer.
		layer = activator.map.GetLayer(activator.x, activator.y, LAYER_FLOOR)

		# There must be floor, and it must have 'unique 1' set.
		if not layer or not layer[0] or not layer[0].f_unique:
			activator.Write("You cannot drop that here; use one of the storage areas.", COLOR_RED)
			SetReturnValue(1)
			return

		guild.log_add("{} dropped {}.".format(activator.name, other.GetName()))
	elif event_num == MEVENT_PUT:
		if is_in_player(WhoAmI()):
			return

		if other.custom_name and other.custom_name[:13].lower() == "rank access: " and guild.member_is_admin(activator.name):
			rank = other.custom_name[13:]

			if not WhoAmI().f_no_pick:
				activator.Write("Only non-pickable containers can be given rank access.", COLOR_RED)
			elif rank == "None":
				activator.Write("The {} is now accessible to all.".format(WhoAmI().GetName()), COLOR_GREEN)
				WhoAmI().title = None
			elif not guild.rank_exists(rank):
				activator.Write("No such rank '{}'.".format(rank), COLOR_RED)
			else:
				activator.Write("The {} is now only accessible to members with the '{}' rank.".format(WhoAmI().GetName(), rank), COLOR_GREEN)
				WhoAmI().title = "[" + rank + "]"

			SetReturnValue(1)
			return

		guild.log_add("{} put {} into the {}.".format(activator.name, other.GetName(), WhoAmI().GetName()))
	# Picking up item, let's check if we can pick it up or not.
	elif event_num == MEVENT_PICK:
		if is_in_player(other):
			return

		if not activator.f_wiz and not guild.member_can_pick(activator.name, other):
			activator.Write("Your rank limits you from picking up the {}. Please see the Guild Storage Manager NPC for more details.".format(other.GetName()), COLOR_BLUE)
			SetReturnValue(1)
			return

		if other.env:
			guild.log_add("{} took {} from the {}.".format(activator.name, other.GetName(), other.env.GetName()))
		else:
			guild.log_add("{} took {}.".format(activator.name, other.GetName()))
	# Don't allow applying anything other than chests.
	elif event_num == MEVENT_APPLY:
		if is_in_player(other):
			return

		if other.type != TYPE_CONTAINER and not other.f_no_pick:
			activator.Write("You must get it first!\n", COLOR_WHITE)
			SetReturnValue(2)
		elif other.type == TYPE_CONTAINER and other.title and guild.member_get_rank(activator.name) != other.title[1:-1] and not guild.member_is_admin(activator.name):
			activator.Write("The {} is only accessible to those with the {} rank.".format(other.GetName(), other.title[1:-1]), COLOR_ORANGE)
			SetReturnValue(2)

guild = Guild(GetOptions())
main()
