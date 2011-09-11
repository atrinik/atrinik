from Atrinik import *
from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest
from Interface import Interface
from Packet import Notification

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
qm = QuestManagerMulti(activator, quest)
inf = Interface(activator, me)

def main():
	is_hello = msg in ("hi", "hey", "hello")

	if not qm.started_part(1):
		if is_hello:
			inf.add_msg("There you are, <i>{}</i>! You're finally awake I see, good, good. I was beginning to worry about you, but you seem to be alright now... unlike my boat.".format(activator.name))
			inf.add_link("Who are you?")

		elif msg == "who are you?":
			inf.add_msg("Huh? Surely you know who I am! Unless... unless the head injury you received during the terrible storm caused you to lose part of your memory... I hope that is not the case.")
			inf.add_link("I don't remember much... what storm?")

		elif msg == "i don't remember much... what storm?":
			inf.add_msg("This is bad luck... First the storm, now this... Well... Let me start from the beginning. My name is {} and you hired me and my boat to transport you to Strakewood Island. You didn't tell me why you had to travel there so urgently, but you offered me a generous sum of gold so I accepted. Unfortunately, a terrible storm caught us while on our way to Strakewood, and you were knocked unconscious. The storm caused large amounts of damage to the boat, but we survived, and got washed up on this small, deserted island...".format(me.name))
			inf.add_link("Deserted island?")

		elif msg == "deserted island?":
			inf.add_msg("That is what it appears like. No civilization or animals that I can see from here, and we have no source of food or clean water.")
			inf.add_link("Should we take a look around?")

		elif msg == "should we take a look around?":
			inf.add_msg("Feel free to, but be careful out there. Try and see if you can find some clean water, as that is the most important thing we need right now. A spring or a small lake would suffice. I'll stay here and examine the damage on the boat. Perhaps we could be able to fix it...")
			inf.add_link("What if I get lost?")

		elif msg == "what if i get lost?":
			inf.add_msg("Hm, good point. We have to be careful out here... Well, take this compass then. We seem to be on the western shore, so you should be able to find your way back. Also, take these torches, as it can get quite dark out there.")
			inf.add_objects([me.FindObject(archname = "compass"), me.FindObject(archname = "torch")])
			Notification(activator.Controller(), "Tutorial Available: Inventory Interaction", "/help basics_inventory_interaction", "?HELP", 90000)
			qm.start(1)

	elif not qm.completed_part(1):
		if is_hello:
			inf.add_msg("Well? Have you found a source of clean water yet? No? Well hurry up then, there has to be a source here somewhere!")

	elif qm.started_part(2) and not qm.completed_part(2):
		if is_hello:
			inf.add_msg("Well? Have you found a source of clean water yet?")
			inf.add_msg("You tell {} about the lake.".format(me.name), COLOR_YELLOW)
			inf.add_msg("Fantastic! Here, take this empty barrel and go fill it up with the water from that lake. We will need it if we are to escape this island.")
			inf.add_objects(me.FindObject(archname = "deserted_island_empty_barrel"))
			Notification(activator.Controller(), "Tutorial Available: Quest List", "/help basics_quest_list", "?HELP", 90000)
			qm.start(3)
			qm.complete(2, sound = False)

	elif qm.started_part(3) and not qm.completed_part(3):
		if is_hello:
			inf.add_msg("Please, take the empty barrel I have given you to the lake you found and fill it up with water.")

	elif qm.started_part(4) and not qm.completed_part(4):
		if is_hello:
			barrel = activator.FindObject(INVENTORY_CONTAINERS, "deserted_island_empty_barrel")

			if barrel:
				inf.add_msg("Good job, {}!".format(activator.name))
				inf.add_msg("You give the water barrel to {}.".format(me.name), COLOR_YELLOW)
				barrel.Remove()

			inf.add_msg("We may be able to escape this island yet. However... we need food as a priority as well. And there is no source of food on the island as far as the eye can see, except some roots, mushrooms, berries and the rare bits of fruit, such as apples.".format(activator.name, me.name))
			inf.add_link("Can't we gather some mushrooms?")

		elif msg == "can't we gather some mushrooms?":
			inf.add_msg("Well, we could, but the amount on the surface would not be enough I fear. We would need many dozens, and big ones as well. Most of the surface ones are not edible either...")
			inf.add_link("What about below the surface?")

		elif msg == "what about below the surface?":
			inf.add_msg("We would need to find a cavern entrance to go underground - one can't just dig out the beach sand or the island dirt and hope they will eventually discover something on a deserted island, that would be very laborious indeed. However, you said the lake was surrounded by a bit of mountains - perhaps there is a cavern in those mountains, maybe right next to the lake. If so, it would be the correct conditions for mushrooms to grow as well.")
			inf.add_link("I'll go have a look around.")

		elif msg == "i'll go have a look around.":
			inf.add_msg("Very well. However, be careful. Even if this looks like a deserted island, you never know... Here, take some more torches, just in case.")
			inf.add_objects(me.FindObject(archname = "torch"))
			qm.start(5)
			qm.complete(4, sound = False)

	elif qm.started_part(5) and not qm.completed_part(5):
		if qm.finished(5):
			if is_hello:
				inf.add_msg("Very good. We have enough mushrooms to last us for a while now. Keep some of those, while I store the rest. There we go. Now, we should think about leaving this island.")
				Notification(activator.Controller(), "Tutorial Available: Hunger", "/help basics_hunger", "?HELP", 90000)
				inf.add_link("How do we do that?")
				qm.complete(5, skip_completion = True)

				mushrooms = me.FindObject(archname = "mushroom1").Clone()
				mushrooms.InsertInto(activator)
		else:
			if is_hello:
				from Language import int2english

				num = qm.num2finish(5)

				inf.add_msg("Ah, you're back soon, it seems! But have you found any mushrooms?")
				inf.add_msg("{} checks how many mushrooms you have found.".format(me.name), COLOR_YELLOW)

				if num == quest["parts"][5 - 1]["num"]:
					inf.add_msg("None? Well, keep looking, there are sure to be some edible ones around here somewhere, perhaps in a nearby cavern...")
				else:
					inf.add_msg("Ah, you have found some! Very good. However, it seems we need at least {} more.".format(int2english(num)))

	elif not qm.started_part(6):
		if is_hello:
			inf.add_msg("We should think about leaving this island soon, {}.".format(activator.name))
			inf.add_link("How do we do that?")

		elif msg == "how do we do that?":
			inf.add_msg("Obviously we need to repair the boat. Luckily this saw survived the storm we went through, however, our wood supplies did not. Which means we need to find some trees, but I can't see anything around here apart from palm trees, which are no good for repairing a boat.")
			inf.add_link("I saw some trees next to the lake.")

		elif msg == "i saw some trees next to the lake.":
			inf.add_msg("We are in luck then! This is good. Here, take this saw then, and bring me some thick branches from those trees. Ten really thick branches should be enough.")
			inf.add_msg("You can interact with the saw while standing next to a tree in order to cut down some branches.", COLOR_YELLOW)
			inf.add_objects(me.FindObject(archname = "sam_goodberry_saw"))

			qm.start(6)

	elif qm.started_part(6) and not qm.completed_part(6):
		if qm.finished(6):
			if is_hello:
				inf.add_msg("Ah, perfect! If you can lend me a hand, we can repair this boat in no time at all with the tree branches that you collected, and then we can set sail.")
				inf.add_link("Where are we heading though?")

			elif msg == "where are we heading though?":
				inf.add_msg("I think we should head for Incuna - we should be close by, as I saw the island in the distance, before the storm hit us. We could take refuge there for a little while, and resupply, in order to continue the journey to Strakewood. And you need to regain your memory... Perhaps learning some basic fighting in Incuna from the townsfolk would refresh your memory.")
				inf.add_link("Let's go then.")

			elif msg == "let's go then.":
				inf.add_msg("It's decided then! Now, help me with repairing this boat, and then we can be off.")
				inf.add_link("Alright then.")

			elif msg == "alright then.":
				saw = activator.FindObject(INVENTORY_CONTAINERS, "sam_goodberry_saw")
				inf.set_title("Sailing the Ocean")
				inf.set_icon(activator.arch.clone.face[0])
				inf.add_msg("You return the saw to {} and help him repair the boat.".format(me.name), COLOR_YELLOW)
				inf.add_msg("After patching up the boat and setting everything in order, you set sail.", COLOR_YELLOW)
				inf.add_msg("You feel tired after all that hard work, perhaps you should speak to {}...".format(me.name), COLOR_YELLOW)
				activator.TeleportTo("/shattered_islands/world_af01", 4, 6, sound = False)
				qm.complete(6, sound = "fanfare6.ogg")
		else:
			inf.add_msg("We need some thick branches to repair the boat, {}.".format(activator.name))


main()
inf.finish()
