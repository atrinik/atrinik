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
