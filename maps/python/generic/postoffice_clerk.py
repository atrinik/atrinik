## @file
## Generic script for post office clerks.

from Atrinik import *
from PostOffice import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = msg.split()
post = PostOffice(activator.name)

## Check for common situations where activator cannot send marked item.
## @param player Who we're sending object to.
## @param object The marked object.
def check_send(player, object):
	# No object?
	if not object:
		me.SayTo(activator, "\nYou need to mark the item you want to send.")
		return False
	# Cannot send locked objects...
	elif object.f_inv_locked:
		me.SayTo(activator, "\nYou must first unlock that item.")
		return False
	# Nor applied ones
	elif object.f_applied:
		me.SayTo(activator, "\nYou must first unapply that item.")
		return False
	# Don't allow sending containers with items inside it.
	elif object.type == TYPE_CONTAINER and object.inv:
		me.SayTo(activator, "\nDue to heightened security levels all items must be removed from containers and sent separately.")
		return False
	# Check if the item can be sent.
	elif not post.can_be_sent(object):
		me.SayTo(activator, "\nYou cannot send that item.")
		return False
	elif object.quickslot:
		me.SayTo(activator, "\nYou must first remove that item from your quickslots.")
		return False
	elif object.type == TYPE_MONEY:
		me.SayTo(activator, "\nI'm terribly sorry, but we do not allow money to be sent.")
		return False
	# Sending to ourselves?
	elif player == activator.name:
		me.SayTo(activator, "\nYou cannot send an item to yourself.")
		return False
	# The player doesn't exist?
	elif not PlayerExists(player):
		me.SayTo(activator, "\nThat player doesn't exist.")
		return False

	return True

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome to our Post Office. You can check if someone has sent you ^items^, or ^send^ an item to someone.\nI can also ^explain^ how to use this post office.")

	# Send an item to someone, showing how much to pay.
	elif text[0] == "send":
		if len(text) > 1:
			marked = activator.FindMarkedObject()
			text[1] = text[1].capitalize()

			if check_send(text[1], marked):
				me.SayTo(activator, "\nIt will cost you {0} to send the '{1}'. If you are pleased with that, say ^sendto {2}^ to send the item.".format(CostString(post.get_price(marked)), marked.GetName(), text[1]))
		else:
			me.SayTo(activator, "\nSend to whom? Do you want me to ^explain^ how to use the post office?")

	# Actually send an item to someone.
	elif text[0] == "sendto" and len(text) > 1:
		marked = activator.FindMarkedObject()
		text[1] = text[1].capitalize()

		if check_send(text[1], marked):
			if activator.PayAmount(post.get_price(marked)):
				activator.Write("You pay {0}.".format(CostString(post.get_price(marked))))
				post.send_item(marked, text[1])
				me.SayTo(activator, "\nThe '{0}' has been sent to {1} successfully.".format(marked.GetName(), text[1]))
				marked.Remove()
			else:
				me.SayTo(activator, "\nYou don't have enough money.")

	# Check for items.
	elif msg == "items":
		items = post.get_items()

		if items:
			me.SayTo(activator, "\nThe following items have been sent to you:\n")

			for i, item in enumerate(post.get_items()):
				activator.Write("#{0} {1} ({2}){3}".format(i + 1, item["name"], item["from"], item["accepted"] and " (accepted)" or " (^accept " + str(i + 1) + "^)"), COLOR_NAVY)

			activator.Write("\nYou can ^accept all^ or ^decline all^.", COLOR_NAVY)
		else:
			me.SayTo(activator, "\nThere is no mail for you right now.")

	# Decline an item, sending it back to the sender.
	elif text[0] == "decline":
		if len(text) > 1:
			if text[1] == "all":
				declined = False

				for i, item in enumerate(post.get_items()):
					if post.can_accept_or_decline(i + 1):
						post.decline_item(i + 1)
						declined = True

				if declined:
					me.SayTo(activator, "\nYou have declined all items.")
			elif text[1].isdigit():
				id = int(text[1])

				if id > 0 and id <= len(post.get_items()) and post.can_accept_or_decline(id):
					post.decline_item(id)
					me.SayTo(activator, "\nYou have declined the item #{0}.".format(id))
		else:
			me.SayTo(activator, "\nDecline what? Check for ^items^ to get ID of the item to decline, and then use \"decline ID\", for example, ^decline 1^ or ^decline all^.")

	# Accept an item.
	elif text[0] == "accept":
		if len(text) > 1:
			if text[1] == "all":
				accepted = False

				for i, item in enumerate(post.get_items()):
					if post.can_accept_or_decline(i + 1):
						post.accept_item(i + 1)
						accepted = True

				if accepted:
					me.SayTo(activator, "\nYou have accepted all items.")
			elif text[1].isdigit():
				id = int(text[1])

				if id > 0 and id <= len(post.get_items()) and post.can_accept_or_decline(id):
					post.accept_item(id)
					me.SayTo(activator, "\nYou have accepted the item #{0}.".format(id))
		else:
			me.SayTo(activator, "\nAccept what? Check for ^items^ to get ID of the item to accept, and then use \"accept ID\", for example, ^accept 1^ or ^accept all^.")

	# Explain how the office works.
	elif msg == "explain":
		me.SayTo(activator, "\nWith post office, you can send items to other players, without the need of them being online. Items you send will stay forever with us, until the player comes and accepts or declines them. If they decline an item, the item will be sent back to the original sender.\nSending an item is simple, just mark the object you want to send, and say \"send <player>\".\nYou can check if someone has sent you ^items^ and if so, you can ^accept^ or ^decline^ them. If you accept, you can get them from one of the mailboxes in this post office.")

try:
	main()
finally:
	post.db.close()
