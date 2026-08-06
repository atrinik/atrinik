"""Limit each Clearhaven mine bomb chest to one bomb per character."""

from Atrinik import *


quest_part = WhoIsOther()

if not quest_part or quest_part.magic != QUEST_STATUS_STARTED:
    SetReturnValue(1)
else:
    marker = "clearhaven_mine_bomb_{}".format(GetOptions())

    if quest_part.ReadKey(marker):
        SetReturnValue(1)
    else:
        quest_part.WriteKey(marker, "1")
