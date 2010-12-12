## @file
## Handles the 'ask show' event for Lynren.

from Atrinik import WhoIsActivator, QUEST_STATUS_COMPLETED, SetReturnValue
from QuestManager import get_quest_object

ob = get_quest_object(WhoIsActivator(), "Rescuing Lynren")

# If we completed the quest, hide the NPC.
if ob and ob.magic == QUEST_STATUS_COMPLETED:
	SetReturnValue(1)
