## @file
## Quests used across several NPCs.

from Atrinik import *

## The shipment of Charob Beer quest.
ShipmentOfCharobBeer = {
	"quest_name": "Shipment of Charob Beer",
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "barrel2.101",
	"item_name": "shipment of Charob Beer",
	"message": "Deliver Charob Beer to the Asterian Arms Tavern.",
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
