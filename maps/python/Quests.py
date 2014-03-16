## @file
## Quests used across several NPCs.

from Atrinik import *
from collections import OrderedDict

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
