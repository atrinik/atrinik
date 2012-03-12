from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest
from Packet import Notification

qm = QuestManagerMulti(activator, quest)

def main():
	SetReturnValue(1)

	if activator.direction != 7:
		return

	pl.DrawInfo("\nYou find an abundance of mushrooms...", COLOR_YELLOW)

	if qm.started_part(5) and not qm.completed_part(5) and not qm.finished(5):
		pl.DrawInfo("Sam Goodberry will be pleased - you should pick up some of the mushrooms and return to him. You reckon seventy mushrooms should be enough...", COLOR_YELLOW)
		Notification(activator.Controller(), "Tutorial Available: Taking and dropping items", "/help basics_taking_dropping", "?HELP", 90000)

main()
