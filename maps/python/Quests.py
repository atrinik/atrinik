## @file
## Quests used across several NPCs.

from Atrinik import *
from collections import OrderedDict

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
    "parts": {
        "speak priest": {
            "message": "Sam Goodberry has suggested that you go speak to the local church priest, who may be able to help you.",
            "type": QUEST_TYPE_SPECIAL,
        },
    },
}

ConstructionOfTelescope = {
    "quest_name": "Construction of Telescope",
    "type": QUEST_TYPE_MULTI,
    "parts": {
        "get shard": {
            "message": "Albar from the Morliana Research Center asked you to go to Brynknot and find a shard from the Great Blue Crystal. He suspects an alchemist or mage might have found such a shard.",
            "arch_name": "blue_crystal_fragment",
            "item_name": "blue crystal fragment",
            "type": QUEST_TYPE_KILL_ITEM,
        },
        "ask about flash": {
            "message": "Albar was very pleased - and confused - with the fragment and the report you received from Jonaslen the mage in Brynknot. He has asked you to go back to Brynknot and ask Jonaslen whether there were reports of a flash in the sky just before the earthquake.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "report about flash": {
            "message": "Jonaslen the mage from Brynknot has told you there were reports about a flash in the sky just before the earthquake from many of the townsfolk, including the guards on duty that day. He also thinks the crystal fell from the sky, which would explain the earthquake and the hole.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "get clear crystal": {
            "message": "Albar has asked you to find a clear crystal for his telescope. He thinks Morg'eean the kobold trader south of Asteria might have some for sale in his little shop.",
            "type": QUEST_TYPE_SPECIAL,
        },
        "get wood": {
            "message": "After delivering the clear crystal to Albar, he has asked to find him wood from the ancient tree Silmedsen who should be south of the Asteria Swamp.",
            "arch_name": "silmedsen_branches",
            "item_name": "Silmedsen's branches",
            "type": QUEST_TYPE_KILL_ITEM,
        },
        "get morliana water": {
            "message": "The ancient tree Silmedsen south of Asteria Swamp has asked you to fill an empty bottle - which he has given you - with water that surrounds the Great Blue Crystal in Morliana.",
            "type": QUEST_TYPE_SPECIAL,
        },
    },
}

RescuingLynren = {
    "quest_name": "Rescuing Lynren",
    "type": QUEST_TYPE_KILL_ITEM,
    "arch_name": "book",
    "item_name": "Lynren's book of holy word",
    "message": "Rescue Lynren the paladin, who is imprisoned in Moroch Temple, by locating her book of holy word in her home in Asteria, among the various temples, and bringing the book to her.",
}
