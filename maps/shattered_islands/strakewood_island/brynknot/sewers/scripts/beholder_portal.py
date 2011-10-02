## @file
## Script for the quest room of the Brynknot Sewers Maze.

from QuestManager import QuestManagerMulti
from Quests import LlwyfenPortal as quest

qm = QuestManagerMulti(activator, quest)

if not qm.finished(4):
	import os.path

	activator.TeleportTo(os.path.dirname(me.map.path) + "/" + me.slaying + "_nyhelobo", me.hp, me.sp, True)
	SetReturnValue(1)
