## @file
## Quest from Mercenary Chereth in the Mercenary Guild on Tutorial
## Island. The quest is to kill Ant Queen and bring back her head as
## proof. As a reward, the player can learn one of the three archery
## skills: bow, crossbow or sling.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

exec(open(CreatePathname("/shattered_islands/scripts/tutorial_island/quests.py")).read())

msg = WhatIsMessage().strip().lower()
text = msg.split()

## Initialize QuestManager.
qm = QuestManager(activator, quest_items["mercenary_chereth"]["info"])

## Common function to finish the quest.
def complete_quest():
	qm.complete()
	me.SayTo(activator, "Here we go!")
	me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Chereth teaches some ancient skill.", COLOR_YELLOW)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started() or not qm.completed():
			if not qm.started():
				me.SayTo(activator, "\nHello, mercenary. I'm Supply Chief Chereth.\nFomerly Archery Commander Chereth, before I lost my eyes.\nWell, I still know a lot about ^archery^.\nPerhaps you want to ^learn^ an archery skill?")
			elif qm.finished():
				me.SayTo(activator, "\nThe head! You have done it!\nNow we can repair the water well.\nSay ^teach^ to me now to learn an archery skill!")
			else:
				me.SayTo(activator, "\nRemember, you need to bring me the proof that you killed the ant queen, then I will ^teach^ you.")
		else:
			me.SayTo(activator, "\nHello {0}.\nGood to see you back.\nI have no quest for you or your ^archery^ skill.".format(activator.name))

	# Explain archery skills
	elif text[0] == "archery":
		me.SayTo(activator, "\nYes, there are three archery skills:\nBow archery is the most common firing arrows.\nSling archery allows fast firing stones with less damage.\nCrossbow archery uses crossbows and bolts. Slow but powerful.")

	# Give out a link to the quest
	elif text[0] == "learn":
		if qm.started() and qm.completed():
			me.SayTo(activator, "\nSorry, I can only teach you |one| archery skill.")
		else:
			me.SayTo(activator, "\nWell, there are three different ^archery^ skills.\nI can teach you only |one| of them.\nYou have to stay with it then. So choose wisely.\nI can tell you more about ^archery^. But before I teach you I have a little ^quest^ for you.")

	# Teach bow archery
	elif msg == "teach me bow":
		if not qm.started() or not qm.finished() or qm.completed():
			me.SayTo(activator, "\nI can't ^teach^ you this now.")
		else:
			complete_quest()

			# Teach the skill
			activator.AcquireSkill(GetSkillNr("bow archery"))

			# Create the player's first bow
			activator.CreateObject("bow_short")
			activator.Write("Chereth gives you a short bow.", COLOR_WHITE)

			# Create some arrows for the player's bow
			activator.CreateObject("arrow", 12)
			activator.Write("Chereth gives you 12 arrows.", COLOR_WHITE)

	# Teach sling archery
	elif msg == "teach me sling":
		if not qm.started() or not qm.finished() or qm.completed():
			me.SayTo(activator, "\nI can't ^teach^ you this now.")
		else:
			complete_quest()

			# Teach the skill
			activator.AcquireSkill(GetSkillNr("sling archery"))

			# Create the player's first sling
			activator.CreateObject("sling_small")
			activator.Write("Chereth gives you a small sling.", COLOR_WHITE)

			# Create some sling stones for the player's sling
			activator.CreateObject("sstone", 12)
			activator.Write("Chereth gives you 12 sling stones.", COLOR_WHITE)

	# Teach crossbow archery
	elif msg == "teach me crossbow":
		if not qm.started() or not qm.finished() or qm.completed():
			me.SayTo(activator, "\nI can't ^teach^ you this now.")
		else:
			complete_quest()

			# Teach the skill
			activator.AcquireSkill(GetSkillNr("crossbow archery"))

			# Create the player's first crossbow
			activator.CreateObject("crossbow_small")
			activator.Write("Chereth gives you a small crossbow.", COLOR_WHITE)

			# Create some bolts for the player's crossbow
			activator.CreateObject("bolt", 12)
			activator.Write("Chereth gives you 12 bolts.", COLOR_WHITE)

	# Give out links to the various archery skills
	elif text[0] == "teach":
		if qm.started() and qm.completed():
			me.SayTo(activator, "\nSorry, I can only teach you |one| archery skill.")
		else:
			if not qm.finished():
				me.SayTo(activator, "\nWhere is the ant queen's head? I don't see it.\nSolve the ^quest^ first and kill the ant queen.\nThen I will teach you.")
			else:
				me.SayTo(activator, "\nAs reward I will teach you an archery skill.\nChoose wisely. I can only teach you |one| of three archery skills.\nDo you want some information about the ^archery^ skills?\nIf you know your choice tell me ^teach me bow^,\n^teach me sling^ or ^teach me crossbow^.")

	# Give out the quest information
	elif text[0] == "quest":
		if qm.started() and qm.completed():
			me.SayTo(activator, "\nI have no quest for you after you helped us out.")
		else:
			if not qm.finished():
				me.SayTo(activator, "\nYes, we need your help first.\nAs supply chief the water support of this outpost is under my command. We noticed last few days problems with our main water source.\nIt seems some giant ants have invaded the caverns under our water well.\nEnter the well next to this house and kill the ant queen!\nBring me her head as a proof and I will ^teach^ you.")

				if not qm.started():
					me.SayTo(activator, "Do you ^accept^ this quest?", 1)
			else:
				me.SayTo(activator, "\nThe head! You have done it!\nNow we can repair the water well.\nSay ^teach^ to me now to learn an archery skill!")

	# Accept the quest.
	elif text[0] == "accept":
		if not qm.started():
			me.SayTo(activator, "\nReturn to me with a proof of your victory and I will reward you.")
			qm.start()
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping us out.")

main()
