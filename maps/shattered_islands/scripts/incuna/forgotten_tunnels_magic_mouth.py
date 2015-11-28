"""Simple script for the quest-related magic mouths in Forgotten Tunnels."""

from Atrinik import *
from QuestManager import QuestManager
from InterfaceQuests import lost_memories

KEY = "forgotten_tunnels_magic_mouth"
qm = QuestManager(activator, lost_memories)

if qm.state_get_bool(KEY) or not qm.started("the_kobolds"):
    SetReturnValue(1)
else:
    qm.state_set(KEY, True)
