## @file
## Handles the 'ask show' event for Lynren.

from QuestManager import QuestManager
from Quests import RescuingLynren as quest

qm = QuestManager(activator, quest)

def main():
	# If we completed the quest, hide the NPC.
	if qm.completed():
		SetReturnValue(1)

main()
