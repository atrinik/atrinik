## @file
## Quests used across several NPCs.

from Atrinik import *

## The shipment of Charob Beer quest.
ShipmentOfCharobBeer = {
	"quest_name": "Shipment of Charob Beer",
	"type": QUEST_TYPE_MULTI,
	"message": "Deliver Charob Beer to the Asterian Arms Tavern.",
	"parts": [
		{
			"message": "Steve Bruck has asked you to deliver a shipment of Charob Beer to the bartender in Asterian Arms Tavern.",
			"type": QUEST_TYPE_KILL_ITEM,
			"arch_name": "barrel2.101",
			"item_name": "shipment of Charob Beer",
		},
		{
			"message": "Gashir, the bartender in Asterian Arms Tavern, was pleased with the delivery, and has suggested that you go speak with Steve Bruck for a payment.",
			"type": QUEST_TYPE_SPECIAL,
		},
	],
}

## The Fort Sether Illness quest.
FortSetherIllness = {
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

EscapingDesertedIsland = {
	"quest_name": "Escaping the Deserted Island",
	"type": QUEST_TYPE_MULTI,
	"message": "",
	"parts": [
		{
			"message": "Explore the deserted island and look for a clean water source.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "You have found a source of clean water. You should report your findings to Sam Goodberry on the western shore of the deserted island.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "Sam Goodberry has given you an empty barrel and asked you to fill it up with clean water from the lake you found, near the eastern shore of the deserted island.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "You have filled the empty barrel up to the brim with clean water, and should return it to Sam Goodberry.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "You have agreed to explore the area around the lake for a cavern, which, if it exists, should have an abundance of edible mushrooms due to the right conditions.",
			"type": QUEST_TYPE_KILL_ITEM,
			"arch_name": "deserted_island_white_mushroom",
			"item_name": "wild white mushroom",
			"num": 70,
		},
		{
			"message": "In order to escape the Deserted Island, you will need to repair the boat. In order to do this, Sam Goodberry has asked you to collect some branches from the trees next to the lake using his saw.",
			"type": QUEST_TYPE_KILL_ITEM,
			"arch_name": "deserted_island_branch",
			"item_name": "thick tree branch",
			"num": 10,
		},
	],
}

LostMemories = {
	"quest_name": "Lost Memories",
	"type": QUEST_TYPE_MULTI,
	"message": "You have reached Incuna, and have started the attempt to get back your lost memories...",
	"parts": [
		{
			"message": "Sam Goodberry has suggested that you go speak to the local church priest, who may be able to help you.",
			"type": QUEST_TYPE_SPECIAL,
		},
	],
}

LairwennsNotes = {
	"quest_name": "Lairwenn's Notes",
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "note",
	"item_name": "Lairwenn's Notes",
	"message": "Lairwenn has lost her notes somewhere in the Brynknot Library and she has asked you to find them for her, as she can't finish the document she is working on without them.",
}

LlwyfenPortal = {
	"quest_name": "Portal of Llwyfen",
	"type": QUEST_TYPE_MULTI,
	"messages": "",
	"parts": [
		{
			"message": "You have found a portal sealed by the power of the elven god Llwyfen. Perhaps you should find a priest of Llwyfen to learn more.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "Maplevale, a priest of Llwyfen and the mayor of Brynknot, has asked you to investigate what's beyond the strange portal in Underground City and has given you an amulet of Llwyfen, which should allow you to pass through the portal.",
			"type": QUEST_TYPE_KILL_ITEM,
			"arch_name": "note",
			"item_name": "Letter from Nyhelobo to oty captain",
		},
		{
			"message": "You have delivered the note you found in the passage beyond the portal in Underground City to Maplevale. After reading it, Maplevale told you to go speak to Talthor Redeye, the captain of the Brynknot guards, immediately.",
			"type": QUEST_TYPE_SPECIAL,
		},
		{
			"message": "Talthor Redeye has given you the key to the lower levels of Brynknot Sewers, and instructed you to find and kill whoever is responsible for the planned attack on Brynknot.",
			"type": QUEST_TYPE_KILL,
			"kills": 1,
		},
	],
}
