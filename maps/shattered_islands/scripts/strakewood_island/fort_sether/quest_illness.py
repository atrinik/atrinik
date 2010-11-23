## @file
## Script for the Illness quest in Fort Sether.

from Atrinik import *
from QuestManager import QuestManagerMulti

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

quest = {
	"quest_name": "Fort Sether Illness",
	"type": QUEST_TYPE_MULTI,
	"message": "Gwenty, a priestess of Grunhilde has asked you to figure out why Fort Sether guards keep falling ill. She seems to suspect it's because of the water, in which case it might be work checking out the water wells.",
	"parts": [
		{
			"message": "Figure out what is causing the illness in Fort Sether.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "You found a kobold named Brownrott below Fort Sether, with a most extraordinary garden. He has shown you a potion he uses to make his garden grow well, and its smell drove you nauseous. Perhaps you should ask Gwenty, the priestess in Fort Sether, for advice.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "Gwenty, the priestess in Fort Sether, has given you a potion to mix with Brownrott's one. He may need some persuading, however...",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "Just as you thought, Brownrott was very reluctant to mix his potion with yours, and has asked you to bring him 10 sword spider hearts first, which can be found by killing sword spiders below Fort Sether.",
			"type": QUEST_TYPE_KILL_ITEM,
			"arch_name": "bone_skull",
			"item_name": "sword spider's heart",
			"num": 10,
		},
		{
			"message": "After delivering the spider hearts to Brownrott, he mixed his potion with yours. You should report to Gwenty for a reward.",
			"type": QUEST_TYPE_SPECIAL,
		},
	],
}

qm = QuestManagerMulti(activator, quest)

## Handle Gwenty the priestess.
def npc_gwenty():
	if not qm.completed():
		# Disable services until the quest has been completed.
		def temple_post_init(self):
			self.services = []

		Temple.TempleGrunhilde.post_init = temple_post_init

		## Create the quest potion.
		def create_potion():
			me.FindObject(name = "Gwenty's Potion").Clone().InsertInto(activator)
			activator.Write("You receive a potion from {0}.".format(me.name))

		# Not started yet.
		if not qm.started_part(1):
			Temple.TempleGrunhilde.hello_msg = "\nApologies, but I'm busy right now with the ^illness^ in Fort Sether, so I'm sorry I can't assist you."

			if msg == "illness":
				me.SayTo(activator, "\nMany guards are falling ill, one after one. Being the only priestess around, I'm quite busy tending the sick guards.\n\n^Do you know the reason?^")
				return
			elif msg == "do you know the reason?":
				me.SayTo(activator, "\nIf only! If I wasn't so busy, I would probably be able to figure it out... Hm, perhaps you could try to figure out the cause of this illness?\n\n^Alright^")
				return
			elif msg == "alright":
				me.SayTo(activator, "\nSplendid! I suspect it has something to do with the water, so I would begin investigating the water wells if I were you.")
				qm.start(1)
				return
		# Talked to the kobold, and haven't asked Gwenty for advice yet.
		elif qm.started_part(2) and not qm.started_part(3):
			activator.Write("You explain about Brownrott and his garden...", COLOR_YELLOW)
			me.SayTo(activator, "\nIndeed? So it was the water, like I thought... Very well, I'll give you a potion for the kobold... And tell him he needs to mix it with his own. It should make his potion harmless when drank but still effective for his garden.")
			create_potion()
			qm.start(3, sound = None)
			qm.complete(2)
			return
		# Asked Gwenty for advice and haven't given the potion to the
		# kobold yet, but lost the potion.
		elif qm.started_part(3) and not qm.completed_part(3) and not activator.FindObject(mode = INVENTORY_CONTAINERS, name = "Gwenty's Potion"):
			me.SayTo(activator, "\nYou lost the potion?\n|Gwenty the priestess sighs heavily.|\nAlright, here is another... Please be more careful with it.")
			create_potion()
			return
		# Persuaded the kobold to mix the potions.
		elif qm.started_part(5):
			activator.Write("You tell Gwenty the priestess the news.", COLOR_YELLOW)
			me.SayTo(activator, "\nExcellent news! Now I can finally offer you my usual services, as I don't have to tend the sick guards anymore. Also, to show my gratitude, please accept this spell from me...")
			activator.AcquireSpell(GetSpellNr("cure disease"), LEARN)
			qm.complete(sound = None)
			return
		# Haven't finished yet.
		else:
			Temple.TempleGrunhilde.hello_msg = "\nApologies, but the illness is still going strong, so until the issue is resolved, I'm afraid I can't help you, as I am busy tending the sick guards."
	else:
		Temple.TempleGrunhilde.hello_msg = "Thank you for your help with the illness problem."

	Temple.handle_temple(Temple.TempleGrunhilde, me, activator, msg)

## Handle Brownrott the kobold.
def npc_brownrott():
	# Agreed to investigate the illness.
	if not qm.completed_part(1):
		if msg == "hi" or msg == "hey" or msg == "hello":
			me.SayTo(activator, "\nWell, hello there! I am {0} the kobold. Don't you think my ^garden^ is beautiful?".format(me.name))

		elif msg == "garden":
			me.SayTo(activator, "\nWhy yes! Just look around you! I use a specially crafted potion that keeps my garden looking nice. Do you want to see?\n\n^Why not...^")

		elif msg == "why not...":
			me.SayTo(activator, "\nHere it is! Doesn't it smell wonderful?")
			activator.Write("You start feeling nauseous as soon as the potion is opened...", COLOR_YELLOW)
			me.SayTo(activator, "Oh right, sorry! Let me close it again... I forgot it has ingredients that make most creatures sick...", True)

			# Now we know the cause of the illness, onto the next part we go.
			if qm.started_part(1):
				activator.Write("As the potion closes, you start feeling better again. Perhaps you should report your findings to Gwenty...", COLOR_YELLOW)
				qm.start(2)
				qm.complete(1, sound = None)
			else:
				activator.Write("As the potion closes, you start feeling better again.", COLOR_YELLOW)

		return
	# Reported to Gwenty.
	elif qm.started_part(3) and not qm.completed_part(3):
		potion = activator.FindObject(mode = INVENTORY_CONTAINERS, name = "Gwenty's Potion")

		# Do we have the potion?
		if potion:
			# Accept Kobold's mini-quest
			if not qm.started_part(4):
				if msg == "hi" or msg == "hey" or msg == "hello":
					me.SayTo(activator, "\nWell, hello there again! My garden is just bea--")
					activator.Write("You interrupt Brownrott and explain to him about the illness in Fort Sether and his potion...", COLOR_YELLOW)
					me.SayTo(activator, "Are you sure? Hmm... I don't know... I don't really trust anyone with my potion except myself... But perhaps... If you bring me something tasty, I might change my mind...\n\n^Alright then^", True)

				elif msg == "alright then":
					me.SayTo(activator, "\nIt's a deal then! Bring me 10 sword spider hearts, and I'll mix your potion with mine. You can find those spiders around in this cave. I usually stay far away from them, but their hearts sure are delicious...")
					qm.start(4)
			# Finished his mini-quest yet?
			elif qm.finished(4):
				if msg == "hi" or msg == "hey" or msg == "hello":
					me.SayTo(activator, "\nMmm! Delicious sword spider hearts! I like you! Alright, let's mix the potions then!")
					activator.Write("You hand the 10 sword spider hearts and the potion to Brownrott.", COLOR_YELLOW)
					potion.Remove()
					qm.start(5)
					qm.complete(4, sound = None)
					qm.complete(3, sound = None)
					me.SayTo(activator, "There! All done. Thank you again for the sword spider hearts!", True)
					activator.Write("You should report the good news to Gwenty.", COLOR_YELLOW)
			# Not yet.
			else:
				if msg == "hi" or msg == "hey" or msg == "hello":
					me.SayTo(activator, "\nBring me 10 sword spider hearts, and I'll mix your potion with mine. You can find those spiders around in this cave. I usually stay far away from them, but their hearts sure are delicious...")

			return

	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWell, hello there again! My garden is just beautiful, isn't it?")

# Handle the kobold.
if me.name == "Brownrott":
	npc_brownrott()
# Handle Fort Sether guards.
elif me.name == "Fortress Guard":
	if msg == "hi" or msg == "hey" or msg == "hello":
		if qm.completed():
			me.SayTo(activator, "\nThank you for solving the illness problem!")
		else:
			me.SayTo(activator, "\nI sure hope this ^illness^ problem gets resolved soon...")

	elif msg == "illness" and not qm.completed():
		me.SayTo(activator, "\nHaven't you heard? You should speak to Gwenty the priestess... You can find her down the stairs in the main building.")
# Handle the priestess.
else:
	import Temple
	npc_gwenty()
