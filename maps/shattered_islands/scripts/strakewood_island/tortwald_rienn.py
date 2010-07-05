## @file
## This script handles the Underground City level 2 side quest.
## Player needs to talk to captured guard, Tortwald, in UC level 2 and
## deliver a letter to the guard's wife, Rienn, in Fort Ghzal. Rienn then
## gives the player another letter that they have to deliver to Tortwald.
## Tortwald then rewards the player with a key.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## Tortwald's quest info.
quest_tortwald = {
	"quest_name": "Tortwald's Letter",
	"type": QUEST_TYPE_SPECIAL,
	"message": "Deliver Tortwald's letter to Rienn in Fort Ghzal.",
}

## Rienn's quest info.
quest_rienn = {
	"quest_name": "Rienn's Letter",
	"type": QUEST_TYPE_SPECIAL,
	"message": "Deliver Rienn's letter to the captured Tortwald the guard in second level of Underground City.",
}

## Create a letter.
def create_letter():
	letter = me.CheckInventory(0, "letter")

	if not letter:
		raise error("Could not find letter inside {0}".format(me.name))

	letter.Clone().InsertInside(activator)
	activator.Write("{0} hands you a letter.".format(me.name), COLOR_ORANGE)

## Give key as a reward.
def create_key():
	key = me.CheckInventory(0, "key_skull")

	if not key:
		raise error("Could not find key inside {0}".format(me.name))

	key.Clone().InsertInside(activator)
	activator.Write("{0} hands you a key.".format(me.name), COLOR_GREEN)

## Handle Tortwald.
def npc_tortwald():
	# Tortwald's quest.
	qm_t = QuestManager(activator, quest_tortwald)

	if msg == "hello" or msg == "hi" or msg == "hey":
		# Not started?
		if not qm_t.started():
			me.SayTo(activator, "\n*sob*... Hello there...\n\n^Who are you?^")
		else:
			# We haven't completed Tortwald's quest?
			if not qm_t.completed():
				# Check for Tortwald's letter.
				letter_t = activator.CheckInventory(2, "letter", "Tortwald's Letter")

				# Lost the letter? Create another one.
				if not letter_t:
					me.SayTo(activator, "\nYou lost the letter I gave you...? I'll write another one...")
					create_letter()
				else:
					me.SayTo(activator, "\nPlease, deliver the letter to Rienn in Fort Ghzal...")
			# We have completed it (by talking to Rienn).
			else:
				# Check if we have Rienn's letter.
				letter_r = activator.CheckInventory(2, "letter", "Rienn's Letter")
				# Rienn's quest.
				qm_r = QuestManager(activator, quest_rienn)

				# We have the letter.
				if letter_r:
					me.SayTo(activator, "\nYou have a letter for me? Oh... It's from Rienn... Thank you...")
					# Remove the letter.
					letter_r.Remove()
					# Complete Rienn's quest.
					qm_r.complete()
					me.SayTo(activator, "Here, take this key... It supposedly opens something around here, but before I could figure it out, I was captured...", 1)
					# Create a key as a reward.
					create_key()
				# Not completed Rienn's quest yet, but we delivered Tortwald's letter.
				elif not qm_r.completed():
					me.SayTo(activator, "\nThank you for delivering my letter...")
				# We completed Rienn's quest.
				else:
					# Player lost the key? Create a new one then.
					if not activator.CheckInventory(2, "key_skull", "Underground City Skull Key"):
						me.SayTo(activator, "\nYou lost the key I gave you... Take this one then, and be more careful with it...")
						create_key()
					else:
						me.SayTo(activator, "\nThank you...")

	# Dialogue when we haven't started Tortwald's quest.
	elif not qm_t.started():
		if msg == "who are you?":
			me.SayTo(activator, "\nI'm {0}, a guard from Fort Ghzal...\n\n^Why are you here?^".format(me.name))

		elif msg == "why are you here?":
			me.SayTo(activator, "\nI was captured by the creatures living in this terrible place... I don't know what they want to do with me...\n\n^Can I help you?^")

		elif msg == "can i help you?":
			me.SayTo(activator, "\nNo... I'm too weak to escape... But... Would you, please, deliver a letter to my wife in Fort Ghzal for me...?\n\n^Yes, I will^\n^No thanks^")

		elif msg == "yes, i will" or msg == "alright then":
			me.SayTo(activator, "\nThank you... Here's the letter. Please deliver it to Rienn in Fort Ghzal...")
			# Start the quest and create the letter.
			qm_t.start()
			create_letter()

		elif msg == "no thanks":
			me.SayTo(activator, "\n*sobs*... Please?\n\n^Alright then^")

## Handle Rienn.
def npc_rienn():
	# Quest manager for Rienn's quest.
	qm_r = QuestManager(activator, quest_rienn)
	# Check if we have Tortwald's letter.
	letter_t = activator.CheckInventory(2, "letter", "Tortwald's Letter")

	if msg == "hello" or msg == "hi" or msg == "hey":
		# Have we completed it? Nothing else to do then.
		if qm_r.completed():
			me.SayTo(activator, "\nThank you, kind adventurer, for your help.")
		# Started, but not completed?
		elif qm_r.started() and not qm_r.completed():
			# We lost the letter? Give them another one.
			if not activator.CheckInventory(2, "letter", "Rienn's Letter"):
				me.SayTo(activator, "\nYou lost the letter? I'll write another one then.")
				create_letter()
			else:
				me.SayTo(activator, "\nPlease deliver my letter to Tortwald...")
		# We have Tortwald's letter.
		elif letter_t:
			me.SayTo(activator, "\nIs that a letter from Tortwald, my beloved husband?")
			activator.Write("You hand the letter to {0}.".format(me.name), COLOR_ORANGE)
			# Remove the letter.
			letter_t.Remove()
			me.SayTo(activator, "... Thank you. Please deliver my letter to him.", 1)
			# Create Rienn's letter.
			create_letter()
			# Start Rienn's quest (but do not play a sound effect).
			qm_r.start(None)

			# Tortwald's quest.
			qm_t = QuestManager(activator, quest_tortwald)
			# Complete Tortwald's quest (with sound effect).
			qm_t.complete()
		# Just a small hint.
		else:
			activator.Write("{0} doesn't seem to notice you and continues to stare in the direction of the ruins north...".format(me.name), COLOR_BLUE)

if me.name == "Tortwald":
	npc_tortwald()
else:
	npc_rienn()
