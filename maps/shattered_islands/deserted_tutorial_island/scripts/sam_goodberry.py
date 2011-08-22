from Atrinik import *
from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
qm = QuestManagerMulti(activator, quest)

def main():
	is_hello = msg in ("hi", "hey", "hello")

	if not qm.started_part(1):
		if is_hello:
			me.SayTo(activator, "\nThere you are, {}! You're finally awake I see, good, good. I was beginning to worry about you, but you seem to be alright now... unlike my boat.\n\n<a><b>Who</b> are you?</a>".format(activator.name))
			activator.Write("Click blue links in NPC dialogue to proceed with the conversation. You can also type out the blue link text contents in the chat input instead, or in the case of sentences, just the bold part of it, such as <b>Who</b> in this case.", "FDD017")

		elif msg == "who" or msg == "who are you?":
			me.SayTo(activator, "\nHuh? Surely you know who I am! Unless... unless the head injury you received during the terrible storm caused you to lose part of your memory... I hope that is not the case.\n\n<a>I don't remember much... what <b>storm</b>?</a>")
			activator.Write("If the conversation gets too lengthy or you just want to see what was said a few moments ago, you can use the scrollbars on the right of the text windows to scroll through the messages.", "FDD017")

		elif msg == "storm" or msg == "i don't remember much... what storm?":
			me.SayTo(activator, "\nThis is bad luck... First the storm, now this... Well... Let me start from the beginning. My name is {} and you hired me and my boat to transport you to Strakewood Island. You didn't tell me why you had to get there urgently, but you paid me well so I accepted. Unfortunately, a terrible storm caught us while on our way to Strakewood, and you were knocked unconscious. The storm caused large amounts of damage to the boat, but we survived, and got washed up on this small, deserted island...\n\n<a><b>Deserted</b> island?</a>".format(me.name))

		elif msg == "deserted" or msg == "deserted island?":
			me.SayTo(activator, "\nThat is what it appears like. No civilization or animals that I can see from here, and we have no source of food or clean water.\n\n<a>Should we take a <b>look</b> around?</a>")

		elif msg == "look" or msg == "should we take a look around?":
			me.SayTo(activator, "\nFeel free to, but be careful out there. Try and see if you can find some clean water, as that is the most important thing we need right now. A spring or a small lake would suffice. I'll stay here and examine the damage on the boat. Perhaps we could be able to fix it...\n\n<a>What if I get <b>lost</b>?</a>")

		elif msg == "lost" or msg == "what if i get lost?":
			me.SayTo(activator, "\nHm, take this compass then. We seem to be on the western shore, so you should be able to find your way back. Also, take these torches, as it can get quite dark out there.")

			compass = me.FindObject(archname = "compass").Clone()
			torches = me.FindObject(archname = "torch").Clone()

			activator.Write("You receive {} and {}es from {}.".format(compass.name, torches.GetName(), me.name), COLOR_YELLOW)
			activator.Write("You can use the compass you received to figure out which way your character is currently facing. In order to interact with items in your inventory, hold <b>Shift</b> to open your inventory. While open, you can navigate your inventory using the arrow keys or the mouse, and use the <b>A</b> key to interact with objects, just like with items below your feet (below inventory). In order to use light sources such as a torch, you must first apply it inside your inventory, and then apply it again, which will ready it.", "FDD017")

			compass.InsertInto(activator)
			torches.InsertInto(activator)
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

main()
