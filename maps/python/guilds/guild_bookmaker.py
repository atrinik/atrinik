## @file
## Used to implement book maker in guilds.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = msg.split()

# Get information for the current user.
info = me.CheckInventory(0, "note", activator.name)

# If the information doesn't exist, create new object.
if not info:
	info = me.CreateObjectInside("note", IDENTIFIED, 1)
	info.name = activator.name

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nHello {}! I'm the guild book maker. If you give me a book, I can overwrite its text!\nUse ^add <your message>^ to add new text. You can use ~<nl>~ to indicate a new line.\nUse ^preview^ to see what your message would look like in the book.\nIf you are not pleased with the preview, you can ^revert^ the message and start over.\nWhen you are done, say ^save^ to save the message into your marked book.\nThere are some tricks to consider when writing books, if you want I can ^explain^ them.".format(activator.name))

	# Explain some useful tricks.
	elif msg == "explain":
		me.SayTo(activator, "\nSome useful tricks to consider when writing books:\n~<b t=\"My Book\">~ would set the book title to |My Book|.\n~<t t=\"Title\">~ would create a title in big letters in the book.\n~<p>~ would finish the previous page, and move onto another.\n\nWhen using regular ~<~ or ~>~ characters in text and not the above tricks, you must escape them with backslashes, like this: ~\<~ and ~\>~.")

	# Make the NPC forget the message that was made so far.
	elif msg == "revert":
		info.msg = ""
		me.SayTo(activator, "\nI have removed everything you have written.")

	# Add a message, replacing "<nl>" with actual newline character.
	elif text[0] == "add":
		if len(text) > 1:
			book_message = WhatIsMessage().strip()

			if book_message.lower().find("endmsg") == -1:
				info.msg += book_message[4:].replace("<nl>", "\n")
				me.SayTo(activator, "\nI have added your message.\nUse ^save^ to save everything you have added so far to your marked book.")
			else:
				activator.Write("Trying to cheat, are we?", COLOR_RED)
				LOG(llevInfo, "CRACK: Player {} tried to write bogus message using guild book maker.\n".format(activator.name))
		else:
			me.SayTo(activator, "\nUse ^add <your message>^, for example, ^add Today is a nice day.^.\n~<nl>~ will automatically get converted to newline.")

	# Preview what the new message would look like.
	elif msg == "preview":
		me.SayTo(activator, "\nText that would appear in your book:\n{}".format(info.msg))

	# Save a message.
	elif msg == "save":
		marked = activator.FindMarkedObject()

		if not marked:
			me.SayTo(activator, "\nFirst mark the book you want to save your message into.")
		elif marked.type != TYPE_BOOK:
			me.SayTo(activator, "\nMarked item is not a book.")
		else:
			marked.msg = info.msg
			info.msg = ""
			# No experience for reading it, level 1, 0 experience book.
			marked.f_no_skill_ident = 1
			marked.level = 1
			marked.exp = 0
			me.SayTo(activator, "\nDone! I have saved your message.")

main()
