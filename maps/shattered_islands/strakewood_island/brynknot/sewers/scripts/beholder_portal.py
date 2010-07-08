## @file
## Script for the quest room of the Brynknot Sewers Maze.

from Atrinik import *
from QuestManager import QuestManager
import os

activator = WhoIsActivator()
me = WhoAmI()

## Talthor's quest.
quest = {
	"quest_name": "Enemies beneath Brynknot",
	"type": QUEST_TYPE_KILL,
	"kills": 1,
	"message": "Go through the Brynknot Sewers Maze, which you can access by taking Llwyfen's portal in Underground City, and kill whoever is responsible for the planned attack on Brynknot, then return to Guard Captain Talthor in Brynknot.",
}

qm = QuestManager(activator, quest)

if not qm.finished():
	activator.TeleportTo(os.path.dirname(me.map.path) + "/sewers_bu_0404", me.hp, me.sp, 1)
	SetReturnValue(1)
