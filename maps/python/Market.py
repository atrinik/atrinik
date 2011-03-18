## @file
## Market related functions.

from Atrinik import *

## Handle merchants.
##
## Merchants sell goods specified in the event's inventory one at a time,
## until their supply runs short.
## @param activator Player.
## @param me The NPC.
## @param event Event object that activated this script. If None, no goods
## or services will be available for purchase.
## @param hello_msg Message to display before the list of goods/services.
## @param hello_msg_after Message to display after the list of goods/services.
def handle_merchant(activator, me, msg, event = None, hello_msg = None, hello_msg_after = None):
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome, dear customer!")

		if hello_msg:
			me.SayTo(activator, hello_msg, True)

		# Display list of goods/services and their cost
		if event:
			# Message in event, create treasure.
			if event.msg:
				import json

				# Parse data, create the treasure.
				for (treasure, num, a_chance) in json.loads(event.msg):
					for i in range(num):
						event.CreateTreasure(treasure, me.level, GT_ONLY_GOOD, a_chance)

				event.msg = None

			# Make sure all objects in the merchant's inventory have
			# stock_nrof attribute, which stores the actual number of
			# items left.
			for obj in event.inv:
				if not obj.ReadKey("stock_nrof"):
					obj.f_identified = True
					obj.WriteKey("stock_nrof", str(max(1, obj.nrof)))
					obj.nrof = 1

			# Create list of goods/services on sale.
			goods = "\n".join("<a>" + obj.GetName() + "</a> for " + CostString(obj.GetCost(obj, COST_TRUE)) for obj in sorted(event.inv, key = lambda obj: len(obj.GetName())) if obj.ReadKey("stock_nrof") != "0")

			if not goods:
				me.SayTo(activator, "Sorry, it seems I am out of stock! Please come back later.", True)
			else:
				me.SayTo(activator, "I can offer you the following:\n" + goods, True)

		if hello_msg_after:
			me.SayTo(activator, hello_msg_after, True)

	elif event:
		# Allowed commands for services
		cmds = {
			"fortune telling": [["fortune", "-s", "fortunes"], ["fortune", "-s"], ["fortune"]],
			"random adage": [["fortune", "-s"], ["fortune"]],
		}

		for obj in event.inv:
			if obj.GetName().lower() == msg:
				stock_nrof = int(obj.ReadKey("stock_nrof"))

				if not obj.f_sys_object:
					# No more goods left.
					if stock_nrof <= 0:
						me.SayTo(activator, "\nOh my, it seems we don't have any {} left... Please come back later.".format(obj.GetName()))
						break

					# Can't carry the item?
					if not activator.Controller().CanCarry(obj.weight + obj.carrying):
						me.SayTo(activator, "\nYou can't carry that...")
						break

				cost = obj.GetCost(obj, COST_TRUE)

				if not activator.PayAmount(cost):
					me.SayTo(activator, "\nSorry, you don't have enough money...")
					break

				activator.Write("You pay {} to {}.".format(CostString(cost), me.name), COLOR_YELLOW)

				# Execute command.
				if obj.f_sys_object:
					import subprocess

					# Default output is in the object's message.
					output = obj.msg

					for cmd in cmds[msg]:
						# Try to execute the command.
						try:
							output = subprocess.check_output(cmd).decode()[:-1].replace("\t", "    ")
						# Command didn't return 0.
						except subprocess.CalledProcessError:
							continue
						# Command not found.
						except OSError:
							continue

						# Success, break out.
						break

					me.SayTo(activator, "\n{}".format(output))
				else:
					clone = obj.Clone()
					clone.WriteKey("stock_nrof")
					activator.Write("{} hands you {}.".format(me.name, clone.GetName()), COLOR_GREEN)
					clone.InsertInto(activator)

					me.SayTo(activator, "\nThank you, pleasure doing business with you!")

					if stock_nrof == 1:
						me.SayTo(activator, "That was the last {} I had in stock too! I'll have to restock...".format(obj.GetName()), True)

					obj.WriteKey("stock_nrof", str(stock_nrof - 1))

				break
