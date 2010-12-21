## @file
## Handles the Construction of Telescope quest.

from Atrinik import *
from QuestManager import QuestManagerMulti

activator = WhoIsActivator()
me = WhoAmI()

quest = {
	"quest_name": "Construction of Telescope",
	"type": QUEST_TYPE_MULTI,
	"message": "",
	"parts": [
		{
			"message": "Albar from the Morliana Research Center asked you to go to Brynknot and find a shard from the Great Blue Crystal. He suspects an alchemist or mage might have found such a shard.",
			"arch_name": "pillar_blue10",
			"item_name": "blue crystal fragment",
			"type": QUEST_TYPE_KILL_ITEM,
		},
		{
			"message": "Albar was very pleased - and confused - with the fragment and the report you received from Jonaslen the mage in Brynknot. He has asked you to go back to Brynknot and ask Jonaslen whether there were reports of a flash in the sky just before the earthquake.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "Jonaslen the mage from Brynknot has told you there were reports about a flash in the sky just before the earthquake from many of the townsfolk, including the guards on duty that day. He also thinks the crystal fell from the sky, which would explain the earthquake and the hole.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "Albar has asked you to find a clear crystal for his telescope. He thinks Morg'eean the kobold trader south of Asteria might have some for sale in his little shop.",
			"arch_name": "jewel_generic",
			"item_name": "clear crystal",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "After delivering the clear crystal to Albar, he has asked to find him wood from the ancient treant Silmedsen who should be south of the Asteria Swamp.",
			"arch_name": "tree_branch1",
			"item_name": "Silmedsen's branches",
			"type": QUEST_TYPE_KILL_ITEM,
		},
		{
			"message": "The ancient treant Silmedsen south of Asteria Swamp has asked you to fill an empty bottle - which he has given you - with water that surrounds the Great Blue Crystal in Morliana.",
			"type": QUEST_TYPE_SPECIAL,
		},
	],
}

qm = QuestManagerMulti(activator, quest)

## Handle Albar in Morliana research center.
def npc_albar():
	if not qm.completed():
		if is_hello:
			if not qm.started_part(1):
				me.SayTo(activator, "\nWell, hello there! Isn't this ^experiment^ simply fascinating?")
			elif not qm.completed_part(1):
				if not qm.finished(1):
					me.SayTo(activator, "\nHave you found the crystal shard yet?")
				else:
					activator.Write("You hand the blue crystal fragment to {} and tell him about the earthquake and the investigation...".format(me.name), COLOR_YELLOW)
					me.SayTo(activator, "\nThank you {}. That is indeed very interesting report you have from Jonaslen... And the fragment! My piece and this fragment look very much alike. It is odd... My piece came from the sky, and the fragment came from the ground?... Hmmm... This is very confusing indeed. Please, go back to Brynknot and ask Jonaslen whether there were reports of a flash in the sky just before the earthquake.".format(activator.name))
					qm.start(2)
					qm.complete(1, sound = False)
			elif qm.started_part(2) and not qm.started_part(4):
				if not qm.completed_part(2):
					me.SayTo(activator, "\nPlease, go back to Brynknot and ask Jonaslen whether there were reports of a flash in the sky just before the earthquake.")
				else:
					activator.Write("You pass the information from Jonaslen the mage to {}.".format(me.name), COLOR_YELLOW)
					me.SayTo(activator, "\nThat makes sense... Quite interesting, it seems the crystal that shattered in Brynknot was the same as the one here in Morliana, the Great Blue Crystal, and both fell from the sky... Well, I need to construct a telescope so I can study the sky to see if there are any more crystals we should know about. But I need some special glass lens crystal first... Would you get it for me, please?\n\n^Sure^")
			elif qm.started_part(4) and not qm.started_part(5):
				obj = activator.FindObject(archname = "jewel_generic", name = "clear crystal")

				if not obj:
					me.SayTo(activator, "\nHave you got the clear crystal from Morg'eean the kobold trader south of Asteria yet?")
				else:
					obj.Decrease()
					activator.Write("You hand one clear crystal to {}.".format(me.name), COLOR_YELLOW)
					me.SayTo(activator, "\nThat's a perfect clear crystal, thank you! Now, I need a stand to mount the telescope on. It needs to be a very sturdy one... The wood from the ancient treant Silmedsen should do. I have heard he was located south of Asteria Swamp...")
					qm.start(5)
					qm.complete(4, sound = False)
			elif qm.started_part(5) and not qm.completed_part(5):
				if not qm.finished(5):
					me.SayTo(activator, "\nHave you found the wood from the ancient treant Silmedsen yet? I have heard he was located south of Asteria Swamp...")
				else:
					activator.Write("You hand the wood from Silmedsen to {}.".format(me.name), COLOR_YELLOW)
					me.SayTo(activator, "\nThat's great wood, just perfect, thank you. Now I can finish the telescope... Please, accept this gift from me. It's some protection against the winter here in the cold North.")
					obj = me.FindObject(archname = "sack").Clone()
					obj.InsertInto(activator)
					activator.Write("You receive {}!".format(obj.name), COLOR_GREEN)
					qm.complete()

		elif not qm.started_part(1):
			if msg == "experiment":
				me.SayTo(activator, "\nYou don't know about it? We are trying to figure out the exact properties of this shard, which came from the Great Blue Crystal. But I assume you know all about that already.\n\n^How's it going?^")
			elif msg == "how's it going?":
				me.SayTo(activator, "\nWell... not very well, actually. We have been looking for more shards of the Great Blue Crystal, but we can't find any. There have been reports about some blue crystal shards in Brynknot, but we have been unable to locate any there. However, it sounds plausible, as there was huge earthquake in Brynknot recently, just like the one that happened on this island when the Great Blue Crystal fell here... Hm...\n\n^Can I help you?^")
			elif msg == "can i help you?":
				me.SayTo(activator, "\nHm... Sorry? Oh, do you really want to help me? Very well then. Please go to Brynknot and find someone who collected a shard of the crystal, and bring it back to me. The owner will probably be a mage or alchemist. I heard there was an alchemist in Brynknot, it could be him...")
				qm.start(1)

		elif qm.completed_part(2) and not qm.started_part(4):
			if msg == "sure":
				me.SayTo(activator, "\nGreat! I have heard Morg'eean the kobold trader south of Asteria trades clear crystals, you should be able to find one in his little shop.")
				qm.start(4)
				qm.complete(3, sound = False)
	else:
		if is_hello:
			me.SayTo(activator, "\nThe telescope is now finished and I can study the sky, all thanks to you, my friend!")

## Handle Jonaslen the researcher in Brynknot.
def npc_jonaslen():
	if qm.started_part(2) and not qm.completed_part(2):
		if is_hello:
			me.SayTo(activator, "\nHello again, {}. I'm still rather busy, so please excuse me...\n\n^Have there been reports about a flash in the sky just before the earthquake?^".format(activator.name))
		elif msg == "have there been reports about a flash in the sky just before the earthquake?":
			me.SayTo(activator, "\nHm... Why yes... Many of the townsfolk, including the guards on duty that day, swore they saw a flash in the sky just before the earthquake... In fact, I think it's possible the crystal that shattered here fell from the sky, which caused the earthquake and the hole. Perhaps you should take this information to Albar, it might help him.")
			qm.start(3)
			qm.complete(2, sound = False)
	elif qm.started_part(1) and not qm.finished(1) and not qm.completed_part(1):
		if is_hello:
			me.SayTo(activator, "\nHello there. I am {}. I'm sorry, but I'm rather busy at the moment, so please excuse me...\n\n^Have you seen any blue crystal shards around?^".format(me.name))
		elif msg == "have you seen any blue crystal shards around?":
			me.SayTo(activator, "\nWhy yes, I have. The recent earthquake opened up a huge hole north of Brynknot, and there were blue shards all around the place. I collected most of them for my own research. But why are you asking this?\n\n^Albar from Morliana Research Center sent me to look for blue crystal shard^")
		elif msg == "albar from morliana research center sent me to look for blue crystal shard":
			me.SayTo(activator, "\nI see. I have heard of the Morliana Research Center. Hm... Very well then. Go back to Albar and tell him that the day the earthquake happened, we went to investigate the hole. Inside was a crystal charged with a tremendous amount of energy, with which it seemed to be protecting itself. We decided to investigate the hole more, but when we came back to look at the crystal, it was shattered completely, and only a few fragments were left. Here, take this fragment and bring it safely to Albar.")
			obj = me.FindObject(name = quest["parts"][0]["item_name"]).Clone()
			obj.InsertInto(activator)
			activator.Write("You receive {}!".format(obj.GetName()), COLOR_GREEN)
	else:
		if is_hello:
			me.SayTo(activator, "\nHello there. I am {}. I'm sorry, but I'm rather busy at the moment, so please excuse me...".format(me.name))

## Handle Silmedsen the ancient tree.
def npc_silmedsen():
	if qm.started_part(5) and not qm.started_part(6):
		if is_hello:
			activator.Write("The tree doesn't seem to notice your presence...\n\n^Can I have some of your branches?^", COLOR_YELLOW)

		elif msg == "can i have some of your branches?":
			activator.Write("The tree shifts slightly, then starts speaking in a deep voice...", COLOR_YELLOW)
			me.SayTo(activator, "\nHo-hum... No... You cannot have my branches... Unless... Unless you could bring me some refreshing water... The swamp water here is not very healthy...\n\n^Alright^")

		elif msg == "alright":
			me.SayTo(activator, "\nGood... I have heard the water that surrounds the Great Blue Crystal in Morliana is clean and fresh... Bring me some in this empty potion bottle and you can have some of my branches...")
			obj = me.FindObject(name = "Silmedsen's potion bottle").Clone()
			obj.InsertInto(activator)
			activator.Write("You receive {} from {}.".format(obj.GetName(), me.name), COLOR_GREEN)
			qm.start(6)
	elif qm.started_part(6) and not qm.completed_part(6):
		if is_hello:
			obj = activator.FindObject(name = "Silmedsen's potion bottle")

			# Do we have the potion, and is it filled?
			if obj.face[0] != "potion_empty.101":
				activator.Write("You hand the bottle full of clean water to {}.".format(me.name), COLOR_YELLOW)
				# Remove the potion.
				obj.Remove()
				me.SayTo(activator, "\nGood... Thank you... Now I can go back to resting... Accept these branches as my gift to you...")
				# Give the player the branches he asked for.
				obj = me.FindObject(name = quest["parts"][4]["item_name"]).Clone()
				obj.InsertInto(activator)
				activator.Write("You receive {} from {}. Albar from Morliana will be pleased...".format(obj.GetName(), me.name), COLOR_GREEN)
			# Not filled yet.
			else:
				me.SayTo(activator, "\nI have heard the water that surrounds the Great Blue Crystal in Morliana is clean and fresh... Bring me some in the empty potion bottle I have given you...")
	else:
		activator.Write("The tree doesn't seem to notice your presence...", COLOR_YELLOW)

## Handle potion the ancient tree gives to the player.
def item_potion():
	SetReturnValue(1)

	# Potion is already filled.
	if me.face[0] != "potion_empty.101":
		activator.Write("The bottle is already filled to the brim.", COLOR_GREEN)
		return

	# Find floor.
	try:
		floor = activator.map.GetLayer(activator.x + freearr_x[activator.facing], activator.y + freearr_y[activator.facing], LAYER_FLOOR)[0]

		# The water from Morliana, adjust the potion.
		if floor.name == "Morliana water":
			me.face = "potion_cyan.101"
			me.value = 10000
			me.f_is_magical = True
			activator.Write("You fill the bottle to the brim with the clear water.", COLOR_GREEN)
			return
	except:
		pass

	activator.Write("You can't fill the bottle with that... You must face the water around the Great Blue Crystal in Morliana.", COLOR_ORANGE)

# Decide what to do.
if GetEventNumber() == EVENT_SAY:
	msg = WhatIsMessage().strip().lower()
	is_hello = msg in ("hi", "hey", "hello")

	# Handle NPCs.
	if me.name == "Albar":
		npc_albar()
	elif me.name.find("Jonaslen") != -1:
		npc_jonaslen()
	elif me.name.find("Silmedsen") != -1:
		npc_silmedsen()
else:
	# Handle the potion.
	if me.name == "Silmedsen's potion bottle":
		item_potion()
