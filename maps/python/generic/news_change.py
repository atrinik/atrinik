## @file
## Implements generic script used for changing news.

from Atrinik import *
from News import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = msg.split()

# Get information for the current user.
info = me.FindObject(0, "note", activator.name)

# If the information doesn't exist, create new object.
if not info:
	info = me.CreateObject("note")
	info.name = activator.name

news = News(info.slaying)

def main():
	global news

	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nHello {0}, I am {1}.\nI can change news in various locations -- like Brynknot, Clearhaven, etc. Full list can be found on sign next to me.\nList of commands:\n^location NEWLOCATION^: Change location for adding/removing messages.\n^messages^: Show messages for chosen location.\n^revert^: Make me forget everything you have written for a message.\n^add MESSAGE^: Add a new message. You can use this multiple times to make longer messages. Use ~<nl>~ to indicate newline.\n^preview^: Preview what your message would look like.\n^save^: Save your complete message as a new message for the chosen location. You can also specify a comma-separated string of locations to save the message into (for example, ~save Brynknot, Greyton~).\n^remove ID^: Remove #ID message from chosen location.\n^remove all^: Remove all messages from chosen location.".format(activator.name, me.name))

	# Change current location.
	elif text[0] == "location" and len(text) > 1:
		location = WhatIsMessage().strip()[9:]
		info.slaying = location
		me.SayTo(activator, "\nChanged location I'm managing for you to '{0}'.".format(location))

	# Show messages.
	elif msg == "messages":
		if not info.slaying:
			me.SayTo(activator, "\nFirst select location you want to get messages for.")
		else:
			messages = news.get_messages()

			if messages:
				me.SayTo(activator, "\nThere are the following messages:\n")

				for i, message in enumerate(messages):
					activator.Write("#{0}: {1}: {2}".format(i + 1, message["time"], message["message"]), COLOR_NAVY)
			else:
				me.SayTo(activator, "\nNo messages in that location.")

	# Make the NPC forget the message that was made so far.
	elif msg == "revert":
		info.msg = ""
		me.SayTo(activator, "\nI have removed everything you have written.")

	# Add a message, replacing "<nl>" with actual newline character.
	elif text[0] == "add" and len(text) > 1:
		news_message = WhatIsMessage().strip()

		if news_message.lower().find("endmsg") == -1:
			info.msg += news_message[4:].replace("<nl>", "\n")
			me.SayTo(activator, "\nI have added your message.")
		else:
			activator.Write("Trying to cheat, are we?", COLOR_RED)
			LOG(llevInfo, "CRACK: Player {0} tried to write bogus message using news changer.\n".format(activator.name))

	# Preview what the new message would look like.
	elif msg == "preview":
		me.SayTo(activator, "\nText that would appear on a sign in chosen location:\n{0}: {1}".format(news.get_time(), info.msg))

	# Save a message.
	elif text[0] == "save":
		if not info.slaying and len(msg) <= 4:
			me.SayTo(activator, "\nFirst select location you want to save the message for.")
		else:
			if len(msg) > 5:
				locations = WhatIsMessage()[5:].strip().split(",")

				news.db.close()
				news = None

				for location in locations:
					try:
						news = News(location.strip())
						news.add_message(info.msg)
					finally:
						news.db.close()
						news = None
			else:
				news.add_message(info.msg)

			info.msg = ""
			me.SayTo(activator, "\nDone! I have added your message.")

	# Remove a message -- either all, or a specified message.
	elif text[0] == "remove" and len(text) > 1:
		if not info.slaying:
			me.SayTo(activator, "\nFirst select location you want to remove message from.")
		elif text[1] == "all":
			news.remove_all_messages()
			me.SayTo(activator, "\nRemoved all messages.")
		elif text[1].isdigit():
			id = int(text[1])

			if id > 0 and id <= len(news.get_messages()):
				news.remove_message(id - 1)
				me.SayTo(activator, "\nRemoved message #{0}.".format(id))

try:
	main()
finally:
	if news:
		news.db.close()
