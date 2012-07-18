## @file
## Script for the quest room of the Brynknot Sewers Maze.

from QuestManager import QuestManagerMulti
from Quests import LlwyfenPortal as quest

qm = QuestManagerMulti(activator, quest)

if not qm.finished(4):
	activator.TeleportTo(me.map.GetPath(me.slaying + "_nyhelobo", True), me.hp, me.sp)
	SetReturnValue(1)
