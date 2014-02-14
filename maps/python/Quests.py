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
    "parts": {
        "figure": {
            "message": "Figure out what is causing the illness in Fort Sether.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "ask advice": {
            "message": "You found a kobold named Brownrott below Fort Sether, with a most extraordinary garden. He has shown you a potion he uses to make his garden grow well, and its smell drove you nauseous. Perhaps you should ask Gwenty, the priestess in Fort Sether, for advice.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "deliver potion": {
            "message": "Gwenty, the priestess in Fort Sether, has given you a potion to mix with Brownrott's one. He may need some persuading, however...",
            "type": QUEST_TYPE_SPECIAL,
        },
        "get hearts": {
            "message": "Just as you thought, Brownrott was very reluctant to mix his potion with yours, and has asked you to bring him 10 sword spider hearts first, which can be found by killing sword spiders below Fort Sether.",
            "type": QUEST_TYPE_KILL_ITEM,
            "arch_name": "bone_skull",
            "item_name": "sword spider's heart",
            "num": 10,
        },
        "reward": {
            "message": "After delivering the spider hearts to Brownrott, he mixed his potion with yours. You should report to Gwenty for a reward.",
            "type": QUEST_TYPE_SPECIAL,
        },
    },
}

EscapingDesertedIsland = {
    "quest_name": "Escaping the Deserted Island",
    "type": QUEST_TYPE_MULTI,
    "message": "",
    "parts": {
        "explore": {
            "message": "Explore the deserted island and look for a clean water source.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "exploration report": {
            "message": "You have found a source of clean water. You should report your findings to Sam Goodberry on the western shore of the deserted island.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "fill barrel": {
            "message": "Sam Goodberry has given you an empty barrel and asked you to fill it up with clean water from the lake you found, near the eastern shore of the deserted island.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "return barrel": {
            "message": "You have filled the empty barrel up to the brim with clean water, and should return it to Sam Goodberry.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "get mushrooms": {
            "message": "You have agreed to explore the area around the lake for a cavern, which, if it exists, should have an abundance of edible mushrooms due to the right conditions.",
            "type": QUEST_TYPE_KILL_ITEM,
            "arch_name": "deserted_island_white_mushroom",
            "item_name": "wild white mushroom",
            "num": 70,
        },
        "get branches": {
            "message": "In order to escape the Deserted Island, you will need to repair the boat. In order to do this, Sam Goodberry has asked you to collect some branches from the trees next to the lake using his saw.",
            "type": QUEST_TYPE_KILL_ITEM,
            "arch_name": "deserted_island_branch",
            "item_name": "thick tree branch",
            "num": 10,
        },
    },
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
            "arch_name": "llwyfen_portal_nyhelobo_note",
            "item_name": "Letter from Nyhelobo to oty captain",
        },
        {
            "message": "You have delivered the note you found in the passage beyond the portal in Underground City to Maplevale. After reading it, Maplevale told you to go speak to Talthor Redeye, the captain of the Brynknot guards, immediately.",
            "type": QUEST_TYPE_SPECIAL,
        },
        {
            "message": "Talthor Redeye has given you the key to the lower levels of Brynknot Sewers, and instructed you to find and kill whoever is responsible for the planned attack on Brynknot.",
            "type": QUEST_TYPE_KILL,
            "num": 1,
        },
    ],
}

TwoLoversDoomed = {
    "quest_name": "Two Lovers Doomed",
    "type": QUEST_TYPE_MULTI,
    "message": "",
    "parts": [
        {
            "message": "You have found the guard Tortwald Howell imprisoned deep inside the Underground City. He has asked you to deliver a letter to his love, Rienn Howell, in Fort Ghzal.",
            "type": QUEST_TYPE_KILL_ITEM,
            "arch_name": "two_lovers_doomed_letter1",
            "item_name": "Tortwald's letter",
        },
        {
            "message": "After delivering Tortwald Howell's letter to Rienn Howell, she has asked you to deliver a letter of her own to Tortwald.",
            "type": QUEST_TYPE_KILL_ITEM,
            "arch_name": "two_lovers_doomed_letter2",
            "item_name": "Rienn's letter",
        },
    ],
}

GallansRevenge = {
    "quest_name": "Galann's Revenge",
    "type": QUEST_TYPE_KILL,
    "kills": 1,
    "message": "Galann Strongfist, the old smith of Brynknot has asked you to go to the Old Outpost north of Aris in the Giant Mountains and kill Torathog the stone giant in revenge.",
}

MelanyesLostWalkingStick = {
    "quest_name": "Melanye's Lost Walking Stick",
    "type": QUEST_TYPE_KILL_ITEM,
    "arch_name": "melanye_walking_stick",
    "item_name": "Melanye's Walking Stick",
    "message": "Melanye in Brynknot Tavern has asked you to bring her back her walking stick, which was stolen in the middle of the night by some evil treant, waking up the old woman, who saw the evil treant running to the east...",
}

ConstructionOfTelescope = {
    "quest_name": "Construction of Telescope",
    "type": QUEST_TYPE_MULTI,
    "message": "",
    "parts": [
        {
            "message": "Albar from the Morliana Research Center asked you to go to Brynknot and find a shard from the Great Blue Crystal. He suspects an alchemist or mage might have found such a shard.",
            "arch_name": "blue_crystal_fragment",
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
            "type": QUEST_TYPE_SPECIAL,
        },
        {
            "message": "After delivering the clear crystal to Albar, he has asked to find him wood from the ancient tree Silmedsen who should be south of the Asteria Swamp.",
            "arch_name": "silmedsen_branches",
            "item_name": "Silmedsen's branches",
            "type": QUEST_TYPE_KILL_ITEM,
        },
        {
            "message": "The ancient tree Silmedsen south of Asteria Swamp has asked you to fill an empty bottle - which he has given you - with water that surrounds the Great Blue Crystal in Morliana.",
            "type": QUEST_TYPE_SPECIAL,
        },
    ],
}

RescuingLynren = {
    "quest_name": "Rescuing Lynren",
    "type": QUEST_TYPE_KILL_ITEM,
    "arch_name": "book",
    "item_name": "Lynren's book of holy word",
    "message": "Rescue Lynren the paladin, who is imprisoned in Moroch Temple, by locating her book of holy word in her home in Asteria, among the various temples, and bringing the book to her.",
}

GandyldsManaCrystal = {
    "quest_name": "Gandyld's Mana Crystal",
    "type": QUEST_TYPE_MULTI,
    "message": "Gandyld, an old mage living east of Aris, has given you his spare mana crystal.",
    "parts": [
        {
            "message": "Since the crystal is rather weak, Gandyld has suggested that you go check out his information about King Rhun's alchemists coming up with a concoction that could enhance mana crystals. King Rhun's domain is in the heart of Giant Mountains, and his outpost is at the very peak of the mountains. The concoction would likely be in a cauldron or a pool in their laboratory.",
            "arch_name": "gandyld_crystal_2",
            "item_name": "Gandyld's Mana Crystal",
            "quest_item_keep": True,
            "type": QUEST_TYPE_KILL_ITEM,
        },
        {
            "message": "Gandyld suspects that since King Rhun likes to experiment with alchemy, he might have found a way to make the concoction even better. If so, it would probably be in his vault or similar.",
            "arch_name": "gandyld_crystal_3",
            "item_name": "Gandyld's Mana Crystal",
            "quest_item_keep": True,
            "type": QUEST_TYPE_KILL_ITEM,
        },
    ],
}
