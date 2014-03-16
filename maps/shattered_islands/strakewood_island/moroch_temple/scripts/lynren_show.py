## @file
## Handles the 'ask show' event for Lynren.

from QuestManager import QuestManager
from InterfaceQuests import rescuing_lynren

# If we completed the quest, hide the NPC.
if QuestManager(activator, rescuing_lynren).completed():
    SetReturnValue(1)
