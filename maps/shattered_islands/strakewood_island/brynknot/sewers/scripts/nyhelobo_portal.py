## @file
## Script for the quest room of the Brynknot Sewers Maze.

from Atrinik import *
from QuestManager import QuestManager
from InterfaceQuests import portal_of_llwyfen

if not QuestManager(activator, portal_of_llwyfen).finished("kill_boss"):
    activator.TeleportTo(me.map.GetPath(me.slaying + "_nyhelobo", True), me.hp, me.sp)
    SetReturnValue(1)
