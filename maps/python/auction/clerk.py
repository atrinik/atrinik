## @file
## Handles Auction House clerks.

from Interface import Interface
from base64 import b64encode, b64decode
from math import ceil
import shelve, random, Auction, re

inf = Interface(activator, me)
db = shelve.open("auction_house_" + me.map.region.name)

## Handles constants for filtering search results in Auction House.
class Filter:
	## @defgroup filter_ids Filtering IDs
	## The various possible filtering IDs.
	## @{

	WEAPONS = 0
	WEAPONS_SLASH = 1
	WEAPONS_PIERCE = 2
	WEAPONS_IMPACT = 3
	WEAPONS_CLEAVE = 4
	SORT_ALPHA = 5
	SORT_ALPHA_REVERSE = 6
	SORT_VALUE = 7
	SORT_VALUE_REVERSE = 8
	ARMOUR = 9
	ARMOUR_MAIL = 10
	ARMOUR_SHIELD = 11
	ARMOUR_HELMET = 12
	ARMOUR_CLOAK = 13
	ARMOUR_BOOTS = 14
	ARMOUR_GLOVES = 15
	ARMOUR_BRACERS = 16
	ARMOUR_GIRDLE = 17
	AMMO = 18
	AMMO_ARROW = 19
	AMMO_BOLT = 20
	AMMO_SSTONE = 21
	AMMO_THROW = 22
	RANGED = 23
	RANGED_BOW = 24
	RANGED_CROSSBOW = 25
	RANGED_SLING = 26
	MAGICAL = 27
	CURSED = 28
	UNCURSED = 29
	IDENTIFIED = 30
	WEAPONS_1H = 31
	WEAPONS_2H = 32
	WEAPONS_POLEARM = 33
	JEWELRY = 34
	JEWELRY_RING = 35
	JEWELRY_AMULET = 36
	JEWELRY_VALUABLE = 37
	SORT_NEWEST = 38
	SORT_OLDEST = 39
	MISC = 40
	## Last member.
	MAX = 41
	## @}

	## Object filters. Any number of these can be applied in filtering.
	## Explanation of the tuple members:
	## - 1: filter name
	## - 2: filter ID; one of @ref filter_ids
	## - 3: list containing filter IDs that will get disabled when the
	##      filter is used.
	filters = [
		("magical", MAGICAL, []),
		("cursed", CURSED, [UNCURSED]),
		("uncursed", UNCURSED, [CURSED]),
		("identified", IDENTIFIED, []),
	]

	## Type filters. Only one of these can be applied at any time.
	## - 1: filter name
	## - 2: filter ID
	## - 3: list containing sub-filters that can be applied when this
	##      filter has been applied (basically recursion). This is a list
	##      containing list of tuples, and only one of the filters in the
	##      list of tuples can be active, but there can be more than one
	##      of these (for example, slash and 2h in the weapons filter).
	types = [
		("weapons", WEAPONS, [
			[
				("slash", WEAPONS_SLASH),
				("pierce", WEAPONS_PIERCE),
				("impact", WEAPONS_IMPACT),
				("cleave", WEAPONS_CLEAVE),
			],
			[
				("1h", WEAPONS_1H),
				("2h", WEAPONS_2H),
				("polearm", WEAPONS_POLEARM),
			],
		]),
		("armour", ARMOUR, [[
			("mail", ARMOUR_MAIL),
			("shield", ARMOUR_SHIELD),
			("helm", ARMOUR_HELMET),
			("cloak", ARMOUR_CLOAK),
			("boots", ARMOUR_BOOTS),
			("gloves", ARMOUR_GLOVES),
			("bracers", ARMOUR_BRACERS),
			("girdle", ARMOUR_GIRDLE),
		]]),
		("ammo", AMMO, [[
			("arrow", AMMO_ARROW),
			("bolt", AMMO_BOLT),
			("sling stone", AMMO_SSTONE),
			("throwing", AMMO_THROW),
		]]),
		("ranged", RANGED, [[
			("bow", RANGED_BOW),
			("crossbow", RANGED_CROSSBOW),
			("sling", RANGED_SLING),
		]]),
		("jewelry", JEWELRY, [[
			("ring", JEWELRY_RING),
			("amulet", JEWELRY_AMULET),
			("valuable", JEWELRY_VALUABLE),
		]]),
		("misc", MISC, []),
	]

	## All possible sorts.
	sorts = [
		("alphabetically", SORT_ALPHA),
		("alphabetically (reverse)", SORT_ALPHA_REVERSE),
		("lower cost first", SORT_VALUE),
		("higher cost first", SORT_VALUE_REVERSE),
		("newest", SORT_NEWEST),
		("oldest", SORT_OLDEST),
	]

	## Weapon sub types.
	weapon_sub_types = [
		# Slash.
		(1, 5, 9),
		# Pierce.
		(2, 6, 10),
		# Impact.
		(0, 4, 8),
		# Cleave.
		(3, 7, 11),
	]

	## All armour types.
	armour_types = (Type.ARMOUR, Type.SHIELD, Type.HELMET, Type.CLOAK, Type.BOOTS, Type.GLOVES, Type.BRACERS, Type.GIRDLE)

	## Ammunition sub types.
	ammo_sub_types = [
		# Arrow.
		(1,),
		# Bolt.
		(2,),
		# Sling stone.
		(3,),
		# Throwing ammo.
		(128, 129, 130, 131),
	]

	## Jewelry types.
	jewelry_types = [
		[Type.RING],
		[Type.AMULET],
		[Type.NUGGET, Type.JEWEL, Type.PEARL, Type.NUGGET],
	]
	# All the above jewelry types in a single list.
	jewelry_all_types = sum(jewelry_types, [])

	## Ranged weapon sub types.
	ranged_sub_types = (0, 1, 2)

## Dummy exception for jumping out of a loop.
class OutOfLoopException(Exception):
	pass

## Find items in the Auction House.
## @param seller Seller name.
## @param namepart String that must be inside the item's name.
## @param filters List of active filters.
def find_items(seller = None, namepart = None, filters = None):
	l = []

	# Go through all the Auction House maps.
	for (path, types, tiles) in db["maps"]:
		m = ReadyMap(path)
		m.timeout = 60 * 10 * 8

		for (x, y) in tiles:
			# Go through the objects on this square.
			for obj in m.GetFirstObject(x, y):
				# Ignore non-item layers.
				if not obj.layer in (LAYER_ITEM, LAYER_ITEM2):
					continue

				# Filter by seller name?
				if seller and obj.ReadKey("auction_house_seller") != seller:
					continue

				# Name part.
				if namepart and obj.GetName().lower().find(namepart) == -1:
					continue

				# Try filters.
				if filters:
					try:
						# Weapons, type must be a weapon for any of the sub-filters to work.
						if Filter.WEAPONS in filters:
							if obj.type != Type.WEAPON:
								continue

							# Check weapon sub types.
							for i in range(len(Filter.weapon_sub_types)):
								if Filter.WEAPONS_SLASH + i in filters and not obj.sub_type in Filter.weapon_sub_types[i]:
									raise OutOfLoopException

							# 1h, 2h and polearms.
							if Filter.WEAPONS_1H in filters and obj.sub_type // len(Filter.weapon_sub_types) != 0:
								continue

							if Filter.WEAPONS_2H in filters and obj.sub_type // len(Filter.weapon_sub_types) != 1:
								continue

							if Filter.WEAPONS_POLEARM in filters and obj.sub_type // len(Filter.weapon_sub_types) != 2:
								continue

						# Armour.
						if Filter.ARMOUR in filters:
							if not obj.type in Filter.armour_types:
								continue

							for i in range(len(Filter.armour_types)):
								if Filter.ARMOUR + 1 + i in filters and obj.type != Filter.armour_types[i]:
									raise OutOfLoopException

						# Ammunition.
						if Filter.AMMO in filters:
							if obj.type != Type.ARROW:
								continue

							for i in range(len(Filter.ammo_sub_types)):
								if Filter.AMMO + 1 + i in filters and not obj.sub_type in Filter.ammo_sub_types[i]:
									raise OutOfLoopException

						# Ranged weapons.
						if Filter.RANGED in filters:
							if obj.type != Type.BOW:
								continue

							for i in range(len(Filter.ranged_sub_types)):
								if Filter.RANGED + 1 + i in filters and obj.sub_type != Filter.ranged_sub_types[i]:
									raise OutOfLoopException

						# Jewelry (rings, amulets, nuggets, jewels).
						if Filter.JEWELRY in filters:
							if not obj.type in Filter.jewelry_all_types:
								continue

							for i in range(len(Filter.jewelry_types)):
								if Filter.JEWELRY + 1 + i in filters and not obj.type in Filter.jewelry_types[i]:
									raise OutOfLoopException

						# Misc (none of the above)
						if Filter.MISC in filters:
							if obj.type == Type.WEAPON or obj.type == Type.ARROW or obj.type == Type.BOW or obj.type in Filter.jewelry_all_types or obj.type in Filter.armour_types:
								continue
					except OutOfLoopException:
						continue

					# All other filters.
					if Filter.MAGICAL in filters and not obj.f_is_magical:
						continue

					if Filter.CURSED in filters and not obj.f_cursed and not obj.f_damned:
						continue

					if Filter.UNCURSED in filters and (obj.f_cursed or obj.f_damned):
						continue

					if Filter.IDENTIFIED in filters and not obj.f_identified:
						continue

				l.append(obj)

	return l

## Find a single item, based on data in tuple 't'.
##
## The data in the tuple is validated, so not just any map path and x,y
## can be checked, only maps inside the Auction House with the x,y in one
## of the boxes, and only items on layer 3/4 that are not 'no_pick 1'.
## @param t Tuple containing the map path, x, y and object UID of the item
## to find.
## @return The item if found, None otherwise.
def find_item(t):
	if not t:
		return None

	# Parse the tuple.
	(map_path, x, y, count) = t

	# Go through the Auction House maps.
	for (path, types, tiles) in db["maps"]:
		# Path doesn't match, go on.
		if path != map_path:
			continue

		# Path matched, but the (x,y) is not in the map's tiles?
		if not (x, y) in tiles:
			break

		# Load up the map.
		m = ReadyMap(path)
		m.timeout = 60 * 10 * 8

		# Try to search for the item.
		for obj in m.GetFirstObject(x, y):
			if obj.layer in (LAYER_ITEM, LAYER_ITEM2) and obj.count == count and not obj.f_no_pick:
				return obj

		break

	return None

## Parsed a base64-encoded string containing map path, x, y and UID of
## object to buy/withdraw/etc.
## @param s The base64-encoded string.
## @return Tuple containing map path, x, y and object UID, None on
## failure.
def parse_base64(s):
	try:
		# Decode the string and construct a tuple.
		(path, x, y, count) = b64decode(s.encode()).decode().split(" ")
		# Convert the integer strings into real integers.
		x = int(x)
		y = int(y)
		count = int(count)
		# Create a new tuple with all the parsed data.
		return (path, x, y, count)
	# Failed to extract the data.
	except:
		return None

## Creates list of objects using client markup.
## @param l The list of objects.
## @param action What action is being done ("buy", "withdraw").
## @param back If not None, contains string to pass in the base64-encoded
## link to buy/withdraw/etc.
## @param sort How to sort the list.
## @param start Where to start in the list; used for paging.
def create_list(l, action, back = None, sort = None, start = None):
	s = ""

	# Sort the list.
	if sort == Filter.SORT_ALPHA:
		l.sort(key = lambda obj: obj.GetName())
	elif sort == Filter.SORT_ALPHA_REVERSE:
		l.sort(key = lambda obj: obj.GetName(), reverse = True)
	elif sort == Filter.SORT_VALUE_REVERSE:
		l.sort(key = lambda obj: int(obj.ReadKey("auction_house_value")), reverse = True)
	elif sort == Filter.SORT_NEWEST:
		l.sort(key = lambda obj: int(obj.ReadKey("auction_house_id")), reverse = True)
	elif sort == Filter.SORT_OLDEST:
		l.sort(key = lambda obj: int(obj.ReadKey("auction_house_id")))
	else:
		l.sort(key = lambda obj: int(obj.ReadKey("auction_house_value")))

	# Cut the list for paging if applicable.
	if start != None:
		l = l[start:start + Auction.PER_PAGE]

	for obj in l:
		if s:
			s += "\n"

		# Create the code.
		code_orig = code = b64encode(" ".join([obj.map.path, str(obj.x), str(obj.y), str(obj.count)]).encode()).decode()

		# Append the 'back' string to the code if possible.
		if back:
			code += " "
			code += b64encode(back.encode()).decode()

		# Add the action link
		s += "[<a=:/t_tell " + action + " " + code + ">" + action + "</a>"

		# If buying, add examine link as well.
		if action == "buy":
			s += ", <a=:/t_tell examine " + code + ">examine</a>"

		# Add the object's name and the cost.
		s += "] " + obj.GetName() + ": <u>" + CostString(int(obj.ReadKey("auction_house_value"))) + "</u> (each)"

		# Buying and there is a stack of items, create links to only buy
		# a part of the stack.
		if action == "buy" and obj.nrof > 1:
			nrof = obj.nrof
			links = []

			for val in [1, 5, 10, 25, 50, 100, 500]:
				if nrof <= val:
					break

				links.append("<a=:/t_tell buy " + code + " " + str(val) + ">" + str(val) + "</a>")

			s += " [" + "; ".join(links) + "]"

	return s

def main():
	if msg == "hello":
		inf.add_msg("Welcome to the Auction House. If you want to buy items from the storage, you'll first need to search for them. Or are you here to sell an item, or see your items in the storage?")
		inf.add_link("I'd like to search for items.", dest = "search1")
		inf.add_link("I'd like to sell an item.", dest = "sell1")
		inf.add_link("I'd like to see my items.", dest = "items")

	elif msg == "search1":
		inf.add_msg("How do you want to search for items? Do you want to search by item name or by seller name?")
		inf.add_msg("You can also browse the following item types: {}".format(", ".join(["<a=:search2_name 1 \"" + str(t[1]) + "\" xx>" + t[0] + "</a>" for t in Filter.types])))
		inf.add_link("I'd like to search by item name.", dest = "search1_name")
		inf.add_link("I'd like to search by seller name.", dest = "search1_seller")

	elif msg == "search1_name":
		inf.add_msg("Please enter (a part of) the item name to search for.")
		inf.set_text_input(prepend = "search2_name 1 \"\" ")

	elif msg.startswith("search2_name "):
		filters = []
		# Use lowest-to-highest value sort by default.
		sort = Filter.SORT_VALUE
		# Page #1.
		page = 1

		match = re.match("(\d+) \"([^\"]*)\" (.+)", msg[13:])

		if not match:
			return

		(page, filters_str, name) = match.groups()
		page = int(page)
		name = name.lower()

		# Parse the filters string, if any.
		if filters_str:
			# Construct a list of available sorting filters.
			sorts = [sort[1] for sort in Filter.sorts]

			for val in filters_str.split(","):
				i = int(val)

				# Is the filter in a valid range?
				if i >= 0 and i < Filter.MAX and not i in filters:
					if i in sorts:
						sort = i

					filters.append(i)

		# No name and no filters.
		if not name and not filters:
			return

		if filters or name == "xx":
			# Construct a list of object type filters.
			type_filters = [t[1] for t in Filter.types]

			# Filters don't have any object type filter, don't allow searching.
			if not [i for i in filters if i in type_filters]:
				return
		elif len(name) < 3:
			inf.add_msg("Your search term must be at least 3 characters, please try again.")
			inf.set_text_input(prepend = "search2_name 1 \"\" ")
			return

		l = find_items(namepart = None if name == "xx" else name, filters = filters)

		# No items and no filters..
		if not l and name and not filters:
			inf.add_msg("Could not find a match for your search term, please try again.")
			inf.set_text_input(prepend = "search2_name 1 \"\" ")
			return

		s = "<yellow>Types:</yellow> "

		for (filter_name, filter_id, filter_subs) in Filter.types:
			if filter_name != Filter.types[0][0]:
				s += ", "

			if filter_id in filters:
				s += "<u>" + filter_name + "</u> <size=8><a=:search2_name 1 \"" + ",".join([str(i) for i in filters if i != filter_id]) + "\" " + name + ">" + "X" + "</a></size>"

				for filter_sub in filter_subs:
					s += " ["

					for (filter_name2, filter_id2) in filter_sub:
						if filter_name2 != filter_sub[0][0]:
							s += ", "

						if filter_id2 in filters:
							s += "<u>" + filter_name2 + "</u> <size=8><a=:search2_name 1 \"" + ",".join([str(i) for i in filters if i != filter_id2]) + "\" " + name + ">" + "X" + "</a></size>"
						else:
							s += "<a=:search2_name 1 \"" + ",".join([str(i) for i in filters if not i in [t[1] for t in filter_sub]] + [str(filter_id2)]) + "\" " + name + ">" + filter_name2 + "</a>"

					s += "]"
			else:
				s += "<a=:search2_name 1 \"" + ",".join([str(i) for i in filters if not i in [t[1] for t in Filter.types]] + [str(filter_id)]) + "\" " + name + ">" + filter_name + "</a>"

		s += "\n<yellow>Filters:</yellow> "

		for (filter_name, filter_id, filter_disables) in Filter.filters:
			if filter_name != Filter.filters[0][0]:
				s += ", "

			if filter_id in filters:
				s += "<u>" + filter_name + "</u> <size=8><a=:search2_name 1 \"" + ",".join([str(i) for i in filters if i != filter_id]) + "\" " + name + ">" + "X" + "</a></size>"
			else:
				s += "<a=:search2_name 1 \"" + ",".join([str(i) for i in filters if not i in filter_disables] + [str(filter_id)]) + "\" " + name + ">" + filter_name + "</a>"

		s += "\n<yellow>Sorting:</yellow> "

		for (filter_name, filter_id) in Filter.sorts:
			if filter_name != Filter.sorts[0][0]:
				s += ", "

			if filter_id == sort:
				s += "<u>" + filter_name + "</u>"
			else:
				s += "<a=:search2_name 1 \"" + ",".join([str(i) for i in filters if i not in sorts] + [str(filter_id)]) + "\" " + name + ">" + filter_name + "</a>"

		s += "\n\nItems matching your search term"

		if name != "xx":
			s += " (" + name + ")"

		s += ":\n"

		if l:
			page = max(1, min(ceil(len(l) / Auction.PER_PAGE), page))
			paging = "\n"

			# Previous button
			if page > 1:
				paging += "<a=:search2_name " + str(page - 1) + " \"" + ",".join([str(i) for i in filters]) + "\" " + name + ">&lt; Previous</a>"
			else:
				paging += "&lt; Previous"

			paging += " | "

			# Next button
			if len(l) > page * Auction.PER_PAGE:
				paging += "<a=:search2_name " + str(page + 1) + " \"" + ",".join([str(i) for i in filters]) + "\" " + name + ">Next &gt;</a>"
			else:
				paging += "Next &gt;"

			# Add the paging and the objects list.
			s += paging
			s += "\n"
			s += create_list(l, "buy", msg, sort, (page - 1) * Auction.PER_PAGE)
			s += paging

		# Help link.
		s += "\n\nClick <a=:helpsearch " + msg + ">here</a> for help with searching."
		inf.add_msg(s)

	elif msg == "search1_seller":
		inf.add_msg("Please enter the seller name to search for.")
		inf.set_text_input(prepend = "search2_seller ")

	elif msg.startswith("search2_seller "):
		name = msg[15:].capitalize()

		if not PlayerExists(name):
			inf.add_msg("No such player.")
			return

		l = find_items(seller = name)

		if not l:
			inf.add_msg("{} does not have any items in this Auction House.".format(name))
		else:
			inf.add_msg("{} has the following items in this Auction House:".format(name))
			inf.add_msg(create_list(l, "buy", msg))

	# Show help about the search interface.
	elif msg.startswith("helpsearch "):
		back = msg[11:]
		inf.add_msg("Several searching methods exist; you can filter by item types (weapons, slash 1h weapons, girdles, etc), item name, etc. When you search for something, all these filtering methods appear at the top of a window similar to this one. Here is what it may look like:\n\n<yellow>Types:</yellow> <u>weapons</u> <size=8><a=:>X</a></size> [<a=:>slash</a>, <u>pierce</u> <size=8><a=:>X</a></size>, ...] [<a=:>1h</a>], <a=:>armour</a>, ...\n\nThe above are item type filters; only one can be active at any time, but more sub-filters can be active along with it. In the above example, the active filter is <b>weapons</b>, so only weapons will appear in your search results, but the <b>pierce</b> sub-filter is also active, so only <b>pierce weapons</b> will appear. Clicking the <b>1h</b> sub-filter would only show <b>1h pierce weapons</b>, but clicking the <b>slash</b> sub-filter would switch from <b>pierce</b> weapons to <b>slash</b> weapons. Clicking the <b>armour</b> filter would deactivate the weapons filter. You can also click the <b>X</b> at top right of the filter name to deactivate that filter.\n\n<yellow>Filters:</yellow> <a=:>magical</a>, <a=:>identified</a>\n\nThe above are item filters, and any number of those can be active. Upon clicking <b>identified</b>, only <b>identified items</b> would appear in your search results.\n\nBelow filtering and sorting is pagination and below that, items list:\n\n&lt; Previous | <a=:>Next &gt;</a>\n[<a=:>buy</a>, <a=:>examine</a>] 10 beer: <u>1 silver coin</u> (each) [<a=:>1</a>; <a=:>5</a>]\n\nIf the <b>Next</b> button is active, it means there is another page of items and you can click it to see them. <b>Previous</b> button would take you to the previous page, if any. Clicking <b>buy</b> would buy the whole stock of the items (price is the shown price multiplied by number of items). You can examine the item by clicking <b>examine</b>. If there is a stock of items and you don't want to buy them all, the numbers like <b>1</b> and <b>5</b> after the item name and cost allow you to buy a smaller quantity.")
		inf.add_link("I'd like to go back to my search.", dest = back)

	# Examine an object.
	elif msg.startswith("examine "):
		# Parse the data.
		try:
			l = WhatIsMessage().strip()[8:].split()
			t = parse_base64(l[0])
			back = b64decode(l[1].encode()).decode()
		except:
			t = None
			back = None

		if not t or not back:
			return

		obj = find_item(t)

		# The object could have been purchased before the player clicked
		# the examine link in their interface.
		if not obj:
			inf.add_msg("That item is not available anymore.")
		else:
			inf.add_msg("<title>{}</title>".format(obj.GetName()))
			inf.add_msg(activator.Controller().Examine(obj, True).strip())
			inf.add_msg("<green>Price:</green> {} (each)\n<green>Seller:</green> {}".format(CostString(int(obj.ReadKey("auction_house_value"))), obj.ReadKey("auction_house_seller")))

		inf.add_link("I'd like to go back to my search.", dest = back)

	# Withdraw an item.
	elif msg.startswith("withdraw "):
		what = msg[9:]

		# Withdraw all items the player is selling.
		if what == "all":
			l = find_items(seller = activator.name)

			if not l:
				inf.add_msg("You do not have any items in this Auction House.")
				return

			# Withdraw all the items.
			for obj in l:
				Auction.clear_custom_values(obj)
				obj.InsertInto(activator)

			inf.add_msg("You have withdrawn {} item(s).".format(len(l)))
		# Withdraw specific item.
		else:
			t = parse_base64(what)

			if not t:
				return

			obj = find_item(t)

			if not obj:
				inf.add_msg("That object is not available anymore.")
			else:
				Auction.clear_custom_values(obj)
				obj.InsertInto(activator)

			inf.add_link("I'd like to go back to my items list.", dest = "items")

	# Buy an item.
	elif msg.startswith("buy "):
		# Parse the data.
		try:
			l = msg[4:].split()
			t = parse_base64(l[0])
			back = b64decode(l[1].encode()).decode()
		except:
			t = None
			back = None

		if not t or not back:
			return

		# Find the item.
		obj = find_item(t)

		if not obj:
			inf.add_msg("That object is not available anymore.")
			inf.add_link("I'd like to go back to my search.", dest = back)
			return

		obj_nrof = max(1, obj.nrof)

		# Try to parse the nrof if any.
		try:
			nrof = int(l[2])
		except:
			nrof = obj_nrof

		nrof = max(1, min(obj_nrof, nrof))
		inf.add_msg(Auction.item_buy(activator, obj, nrof, obj.ReadKey("auction_house_seller")))
		inf.add_link("I'd like to go back to my search.", dest = back)

	# Sell an item.
	elif msg == "sell1":
		marked = activator.Controller().FindMarkedObject()

		if not marked:
			inf.add_msg("Please mark the item that you want to sell first.")
			return

		inf.add_msg("<title>{}</title>".format(marked.GetName()))
		inf.add_msg(activator.Controller().Examine(marked, True).strip())
		inf.add_msg("How much do you want to sell that for? Example: <green>10 gold, 50 silver</green>".format(marked.GetName()))
		inf.set_text_input(prepend = "sell2 ")

	elif msg.startswith("sell2 "):
		marked = activator.Controller().FindMarkedObject()

		if not marked:
			return

		# Parse the cost.
		val = Auction.string_to_cost(msg[6:])

		if not val:
			inf.add_msg("Invalid value to sell for, please try again. Example: <green>10 gold, 50 silver</green>")
			inf.set_text_input(prepend = "sell2 ")
			return

		# Over the max?
		if val * max(1, marked.nrof) > Auction.MAX_VALUE:
			inf.add_msg("I am sorry, but the combined value of the items you want to sell cannot exceed {}.".format(CostString(Auction.MAX_VALUE)))
			return

		# Cannot sell locked items.
		if marked.f_inv_locked:
			inf.add_msg("Please unlock that item from your inventory first.")
			return
		# Or containers with items.
		elif marked.type == Type.CONTAINER and marked.inv:
			inf.add_msg("Due to heightened security levels all items must be removed from containers and sold separately.")
			return
		elif marked.f_startequip or marked.f_no_drop or marked.f_quest_item or marked.f_sys_object or marked.weight == 0:
			inf.add_msg("Sorry, we can't accept that item.")
			return
		elif marked.f_applied:
			inf.add_msg("You must first unapply that item.")
			return
		elif marked.quickslot:
			inf.add_msg("You must first remove that item from your quickslots.")
			return
		elif marked.type == Type.MONEY:
			inf.add_msg("Sorry, selling money is not allowed in this Auction House.")
			return

		# Check if the player reached the limit yet.
		if len(find_items(seller = activator.name)) >= Auction.MAX_ITEMS:
			inf.add_msg("You already have {} items in this Auction House. You must either withdraw them or wait until someone buys them.".format(Auction.MAX_ITEMS))
			inf.add_link("I'd like to see my items.", dest = "items")
			return

		# Look for correct box to place the item.
		for (path, types, tiles) in db["maps"]:
			# Incorrect object type, go on.
			if not marked.type in types and not "other" in types:
				continue

			# Ready the map.
			m = ReadyMap(path)
			m.timeout = 60 * 10 * 8
			t = None

			# Choose first tile that doesn't have any items yet.
			for (x, y) in tiles:
				if m.GetLayer(x, y, LAYER_ITEM) or m.GetLayer(x, y, LAYER_ITEM2):
					continue

				t = (x, y)
				break

			# No tile with no items, choose one randomly.
			if not t:
				t = random.choice(tiles)

			(x, y) = t
			inf.add_msg("Your {} has been placed in this Auction House successfully.".format(marked.GetName()))
			inf.add_msg("Selling price: {} (each)".format(CostString(val)))
			inf.add_link("I'd like to see my items.", dest = "items")

			marked.WriteKey("auction_house_seller", activator.name)
			marked.WriteKey("auction_house_value", str(val))
			marked.WriteKey("auction_house_id", str(db["uid"]))
			auction_uid_increase()
			m.Insert(marked, x, y)
			return

		inf.add_msg("I am sorry, it seems we do not accept items like your {} in this Auction House.".format(marked.GetName()))

	elif msg == "items":
		l = find_items(seller = activator.name)

		if not l:
			inf.add_msg("You do not have any items in this Auction House.")
		else:
			inf.add_msg("Your items in this Auction House:")
			inf.add_msg(create_list(l, "withdraw", sort = Filter.SORT_ALPHA))
			inf.add_link("I'd like to withdraw all my items.", dest = "withdraw all")

	# Allow setting up the shop if the player is a DM.
	elif activator.f_wiz and msg == "setup":
		import json
		temp = []
		sign = None

		# Try and find the sign object on current map.
		try:
			for x in range(me.map.width):
				for y in range(me.map.height):
					for obj in me.map.GetLayer(x, y, LAYER_SYS):
						if obj.name == "Auction House sign":
							sign = obj
							raise OutOfLoopException
		except OutOfLoopException:
			pass

		if not sign:
			print("Could not find Auction House information sign on current map, bailing out.")
			return

		# Load the data from the sign's message using json.
		for (path, types) in json.loads(sign.msg):
			m = ReadyMap(path)
			t = (path, [], [])

			# Look for unique floor tiles on the map and append them to a list in
			# the above tuple.
			for x in range(m.width):
				for y in range(m.height):
					try:
						floor = m.GetLayer(x, y, LAYER_FLOOR)[0]

						if floor.f_unique:
							t[2].append((x, y))
					except:
						pass

			# Parse the types.
			for token in types.split(","):
				token = token.strip()

				if token == "other":
					t[1].append(token)
				else:
					t[1].append(Type.__dict__[token])

			temp.append(t)

		# Update the database.
		db["maps"] = temp
		# Reset UID counter.
		db["uid"] = 0

		# Assign UID to all objects in the Auction House.
		for obj in find_items():
			obj.WriteKey("auction_house_id", str(db["uid"]))
			auction_uid_increase()

## Increase the Auction House UID counter by 1.
def auction_uid_increase():
	temp = db["uid"]
	temp += 1
	db["uid"] = temp

try:
	main()
	inf.finish()
# Make sure to close the database.
finally:
	db.close()
