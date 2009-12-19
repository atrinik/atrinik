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
	# Check if the item can be sent.
	elif not post.can_be_sent(object):
		me.SayTo(activator, "\nYou cannot send that item.")
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

# Send an item to someone, showing how much to pay.
if text[0] == "send":
	if len(text) > 1:
		marked = activator.FindMarkedObject()
		text[1] = text[1].title()

		if check_send(text[1], marked):
			me.SayTo(activator, "\nIt will cost you %s to send the '%s'. If you are pleased with that, say ^sendto %s^ to send the item." % (activator.ShowCost(post.get_price(marked)), marked.GetName(), text[1]))
	else:
		me.SayTo(activator, "\nSend to whom? Do you want me to ^explain^ how to use the post office?")

# Actually send an item to someone.
elif text[0] == "sendto" and len(text) > 1:
	marked = activator.FindMarkedObject()
	text[1] = text[1].title()

	if check_send(text[1], marked):
		if activator.PayAmount(post.get_price(marked)):
			activator.Write("You pay %s." % activator.ShowCost(post.get_price(marked)))
			post.send_item(marked, text[1])
			me.SayTo(activator, "\nThe '%s' has been sent to %s successfully." % (marked.GetName(), text[1]))
			marked.Remove()
		else:
			me.SayTo(activator, "\nYou don't have enough money.")

# Check for items.
elif msg == "items":
	items = post.get_items()

	if items:
		me.SayTo(activator, "\nThe following items have been sent to you:\n")

		for i, item in enumerate(post.get_items()):
			activator.Write("#%d %s (%s)%s" % (i + 1, item["name"], item["from"], item["accepted"] and " (accepted)" or ""), COLOR_NAVY)
	else:
		me.SayTo(activator, "\nThere is no mail for you right now.")

# Decline an item, sending it back to the sender.
elif text[0] == "decline":
	if len(text) > 1:
		id = int(text[1])

		if id > 0 and id <= len(post.get_items()) and post.can_accept_or_decline(id):
			post.decline_item(id)
			me.SayTo(activator, "\nYou have declined the item #%d." % id)
	else:
		me.SayTo(activator, "\nDecline what? Check for ^items^ to get ID of the item to decline.")

# Accept an item.
elif text[0] == "accept":
	if len(text) > 1:
		id = int(text[1])

		if id > 0 and id <= len(post.get_items()) and post.can_accept_or_decline(id):
			post.accept_item(id)
			me.SayTo(activator, "\nYou have accepted the item #%d." % id)
	else:
		me.SayTo(activator, "\nAccept what? Check for ^items^ to get ID of the item to accept.")

# Explain how the office works.
elif msg == "explain":
	me.SayTo(activator, "\nWith post office, you can send items to other players, without the need of them being online. Items you send will stay forever with us, until the player comes and accepts or declines them. If they decline an item, the item will be sent back to the original sender.\nSending an item is simple, just mark the object you want to send, and say \"send <player>\".\nYou can check if someone has sent you ^items^ and if so, you can ^accept^ or ^decline^ them. If you accept, you can get them from one of the mailboxes in this post office.")

elif msg == "hi" or msg == "hey" or msg == "hello":
	me.SayTo(activator, "\nWelcome to our Post Office. You can check if someone has sent you ^items^, or ^send^ an item to someone.\nI can also ^explain^ how to use this post office.")

post.db.close()
