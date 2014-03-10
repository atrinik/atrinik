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

RescuingLynren = {
    "quest_name": "Rescuing Lynren",
    "type": QUEST_TYPE_KILL_ITEM,
    "arch_name": "book",
    "item_name": "Lynren's book of holy word",
    "message": "Rescue Lynren the paladin, who is imprisoned in Moroch Temple, by locating her book of holy word in her home in Asteria, among the various temples, and bringing the book to her.",
}
