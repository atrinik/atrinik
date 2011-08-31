from Atrinik import *
from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest
from Interface import Interface

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
			inf.add_msg("Click the blue links to proceed with the conversation, or press its shortcut number on the keyboard.", "FDD017")
			inf.add_link("Who are you?")

		elif msg == "who are you?":
			inf.add_msg("Huh? Surely you know who I am! Unless... unless the head injury you received during the terrible storm caused you to lose part of your memory... I hope that is not the case.")
			inf.add_link("I don't remember much... what storm?")

		elif msg == "i don't remember much... what storm?":
			inf.add_msg("This is bad luck... First the storm, now this... Well... Let me start from the beginning. My name is {} and you hired me and my boat to transport you to Strakewood Island. You didn't tell me why you had to travel there so urgently, but you offered me a nice sum of gold so I accepted. Unfortunately, a terrible storm caught us while on our way to Strakewood, and you were knocked unconscious. The storm caused large amounts of damage to the boat, but we survived, and got washed up on this small, deserted island...".format(me.name))
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
			inf.add_msg("You can use the compass you received to figure out which way your character is currently facing. In order to interact with items in your inventory, hold <b>Shift</b> to open your inventory. While open, you can navigate your inventory using the arrow keys or the mouse, and use the <b>A</b> key to interact with objects, just like with items below your feet (below inventory). In order to use light sources such as a torch, you must first apply it inside your inventory, and then apply it again, which will ready it.", "FDD017")
			qm.start(1)

	elif not qm.completed_part(1):
		if is_hello:
			me.SayTo(activator, "\nWell? Have you found a source of clean water yet? No? Well hurry up then, there has to be a source here somewhere!")

	elif qm.started_part(2) and not qm.completed_part(2):
		if is_hello:
			me.SayTo(activator, "\nWell? Have you found a source of clean water yet?\n<yellow>You tell {} about the lake.</yellow>\nFantastic! Here, take this empty barrel and go fill it up with the water from that lake. We will need it if we are to escape this island.".format(me.name))
			obj = me.FindObject(archname = "deserted_island_empty_barrel").Clone()
			obj.InsertInto(activator)
			activator.Write("You receive {} from {}.".format(obj.name, me.name), COLOR_YELLOW)
			activator.Write("You can use the <b>E</b> key to examine items below your feet or in your inventory. Examining items reveals more detailed description about the item - for example, if you drop the so-called god-given items, they will vanish forever - but often there is a way to get them again, if they were important. To prevent losing important items however, even non-god-given ones, you can lock them using the <b>L</b> key - the same key will also unlock a locked item. Locked items cannot be dropped, even by accident.", "FDD017")
			qm.start(3)
			qm.complete(2, sound = False)

	elif qm.started_part(3) and not qm.completed_part(3):
		if is_hello:
			me.SayTo(activator, "\nPlease, take the empty barrel I have given you to the lake you found and fill it up with water.")

	elif qm.started_part(4) and not qm.completed_part(4):
		if is_hello:
			barrel = activator.FindObject(INVENTORY_CONTAINERS, "deserted_island_empty_barrel")

			if barrel:
				me.SayTo(activator, "\nGood job, {}!\n<yellow>You give the water barrel to {}.</yellow>".format(activator.name, me.name))
				barrel.Remove()
			else:
				me.SayTo(activator, "")

			me.SayTo(activator, "We may be able to escape this island yet. However... we need food as a priority as well. And there is no source of food on the island as far as the eye can see, except some roots, mushrooms, berries and the rare bits of fruit, such as apples.\n\n<a>Can't we gather some <b>mushrooms</b>?</a>".format(activator.name, me.name), True)

		elif msg == "mushrooms" or msg == "can't we gather some mushrooms?":
			me.SayTo(activator, "\nWell, we could, but the amount on the surface would not be enough I fear. We would need many dozens, and big ones as well. Most of the surface ones are not edible either...\n\n<a>What about <b>below</b> the surface?</a>")

		elif msg == "below" or msg == "what about below the surface?":
			me.SayTo(activator, "\nWe would need to find a cavern entrance to go underground - one can't just dig out the beach sand or the island dirt and hope they will eventually discover something on a deserted island, that would be very laborious indeed. However, you said the lake was surrounded by a bit of mountains - perhaps there is a cavern in those mountains, maybe right next to the lake. If so, it would be the correct conditions for mushrooms to grow as well.\n\n<a>I'll go have a <b>look</b> around</a>")

		elif msg == "look" or msg == "i'll go have a look around":
			torches = me.FindObject(archname = "torch").Clone()
			sack = me.FindObject(archname = "sack").Clone()

			me.SayTo(activator, "\nVery well. However, be careful. Even if this looks like a deserted island, you never know... Here, take some more torches, just in case. Also, take this sack so you have some place to put the mushrooms, if you find any.\n<yellow>{} hands you {}es and {}.</yellow>".format(me.name, torches.GetName(), sack.GetName()))
			activator.Write("Containers are useful for organizing items inside your inventory - for example, food container. Use <b>A</b> (apply) to ready a container in your inventory. Another apply will open the container and you can see the items inside. You can also open containers below your feet, and this only requires one apply.", "FDD017")

			torches.InsertInto(activator)
			sack.InsertInto(activator)

			qm.start(5)
			qm.complete(4, sound = False)

	elif qm.started_part(5) and not qm.completed_part(5):
		if qm.finished(5):
			if is_hello:
				me.SayTo(activator, "\nVery good. We have enough mushrooms to last us for a while. Keep some of those, while I store the rest. There we go. Now, we should think about leaving this island.\n\n<a><b>How</b> do we do that?</a>")
				activator.Write("Eating food is important - watch your food bar, because when it empties, you will start starving, which stops your health regeneration, and you slowly start losing health. If this happens, your character will blindly grab for some food in your inventory, even if unidentified, which can be dangerous, as the food could be poisonous. Thus it is important to always identify items you find in the game before using them.", "FDD017")
				qm.complete(5, skip_completion = True)

				mushrooms = me.FindObject(archname = "mushroom1").Clone()
				mushrooms.InsertInto(activator)
		else:
			if is_hello:
				from Language import int2english

				num = qm.num2finish(5)

				me.SayTo(activator, "\nAh, you're back soon, it seems! But have you found any mushrooms?")
				activator.Write("{} checks how many mushrooms you have found.".format(me.name), COLOR_YELLOW)

				if num == quest["parts"][5 - 1]["num"]:
					me.SayTo(activator, "None? Well, keep looking, there are sure to be some edible ones around here somewhere, perhaps in a nearby cavern...", True)
				else:
					me.SayTo(activator, "Ah, you have found some! Very good. However, it seems we need at least {} more.".format(int2english(num)), True)

	elif not qm.started_part(6):
		if is_hello:
			me.SayTo(activator, "\nWe should think about leaving this island soon, {}.\n\n<a><b>How</b> do we do that?</a>".format(activator.name))

		elif msg == "how" or msg == "how do we do that?":
			me.SayTo(activator, "\nObviously we need to repair the boat. Luckily this saw survived the storm we went through, however, our wood supplies did not. Which means we need to find some trees, but I can't see anything around here apart from palm trees, which are no good for repairing a boat.\n\n<a>I saw some <b>trees</b> next to the lake</a>")

		elif msg == "trees" or msg == "i saw some trees next to the lake":
			saw = me.FindObject(archname = "sam_goodberry_saw").Clone()

			me.SayTo(activator, "\nWe are in luck then! This is good. Here, take this saw then, and bring me some thick branches from those trees. Ten really thick branches should be enough.")
			activator.Write("{} gives you a saw.".format(me.name), COLOR_YELLOW)
			activator.Write("You can apply (<b>A key</b>) the saw while standing next to a tree in order to cut down some branches.", "FDD017")

			saw.InsertInto(activator)

			qm.start(6)

	elif qm.started_part(6) and not qm.completed_part(6):
		if qm.finished(6):
			if is_hello:
				me.SayTo(activator, "\nAh, perfect! If you can lend me a hand, we can repair this boat in no time at all with the tree branches that you collected.\n\n<a>Where are we <b>heading</b> though?</a>")

			elif msg == "heading" or msg == "where are we heading though?":
				me.SayTo(activator, "\nI think we should head for Incuna - we should be close by, as I saw the island in the distance, before the storm hit us. We could take refuge there for a little while, and resupply, in order to continue the journey to Strakewood. And you need to regain your memory... Perhaps learning some basic fighting in Incuna from the townsfolk would refresh your memory.\n\n<a>Let's <b>go</b> then</a>")

			elif msg == "go" or msg == "let's go then":
				me.SayTo(activator, "\nIt's decided then! Now, help me with repairing this boat, and then we can be off.\n\n<a><b>Alright</b> then</a>")

			elif msg == "alright" or msg == "alright then":
				saw = activator.FindObject(INVENTORY_CONTAINERS, "sam_goodberry_saw")
				saw.Remove()
				qm.complete(6, sound = "fanfare6.ogg")
		else:
			me.SayTo(activator, "\nWe need some thick branches to repair the boat, {}.".format(activator.name))


main()
inf.finish()
