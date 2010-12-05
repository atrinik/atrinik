## @file
## API for handling taverns.

from Atrinik import CostString, COLOR_YELLOW

## Handle bartenders.
##
## Bartenders allow players to purchase various provisions, any number at
## a time (up to 'num_max'). Provisions available for purchase are
## defined as usual objects inside the event's inventory (from which they
## are cloned when purchased).
## @param activator Player.
## @param me The NPC.
## @param event Event object that activated this script. If None, no provisions
## will be available for purchase.
## @param ignore List of provision names to ignore.
## @param hello_msg Message to display before the list of provisions.
## @param hello_msg_after Message to display after the list of provisions.
## @param num_max Maximum number of provisions a player can purchase at a
## time.
def handle_bartender(activator, me, msg, event = None, ignore = [], hello_msg = None, hello_msg_after = None, num_max = 100):
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome to my tavern, dear customer! I am {}.".format(me.name))

		if hello_msg:
			me.SayTo(activator, hello_msg, True)

		# Display list of provisions if applicable.
		if event:
			# The list of provisions is loaded from the event's inventory
			# and sorted by name, and a table for display is built out of
			# the provisions not in 'ignore', along with the cost of each.
			me.SayTo(activator, "I can offer you the following ^provisions^:\n\n" + "\n".join("^" + x.name + "^ for " + CostString(x.value) for x in sorted(event.inv, key = lambda obj: obj.name) if not x.name in ignore), True)

		if hello_msg_after:
			me.SayTo(activator, hello_msg_after, True)

	# Display information about provisions.
	elif event and msg == "provisions":
		me.SayTo(activator, "\nYou can either buy the provisions one by one, or prefix the provision name with a number (maximum {0}), for example:\n^{0} {1}^".format(min(num_max, 10), event.inv.name))

	# See if the message matched one of the provision names.
	elif event:
		# Try to parse number of provisions to buy from the message.
		try:
			# Split the string and get the (possible) number.
			num_str = msg.split(" ", 1)[0]
			# Parse the number into integer, but add limit.
			num = min(int(num_str), num_max)
			# Adjust the message, removing the number.
			msg = msg[len(num_str):].strip()
		except:
			num = 1

		# Search for the provision name.
		for obj in event.inv:
			# Matched?
			if obj.name.lower() == msg:
				# Make sure the player can actually carry the provisions.
				if not activator.Controller().CanCarry(obj.weight * num + obj.carrying):
					me.SayTo(activator, "\nYou can't carry {} {}...".format(num, obj.name))
				# Pay the amount.
				elif activator.PayAmount(obj.value * num):
					activator.Write("You pay {} to {}.".format(CostString(obj.value * num), me.name), COLOR_YELLOW)
					me.SayTo(activator, "\nHere you go, {} {}, just as you ordered!\nPleasure doing business with you!".format(num, obj.name))
					# Clone the object.
					clone = obj.Clone()
					# Adjust the nrof.
					clone.nrof = num
					# Insert the clone into the player.
					clone.InsertInto(activator)
				else:
					me.SayTo(activator, "\nSorry, you don't have enough money...")

				# Done searching.
				break
