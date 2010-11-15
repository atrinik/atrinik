## @file
## Handles the 'ask show' event for Lynren.

from Atrinik import WhoIsActivator, QUEST_STATUS_COMPLETED, SetReturnValue

ob = WhoIsActivator().GetQuestObject("Rescuing Lynren")

# If we completed the quest, hide the NPC.
if ob and ob.magic == QUEST_STATUS_COMPLETED:
	SetReturnValue(1)
