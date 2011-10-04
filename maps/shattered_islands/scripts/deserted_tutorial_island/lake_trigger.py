from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest

qm = QuestManagerMulti(activator, quest)

def main():
	SetReturnValue(1)

	if qm.started_part(1) and not qm.completed_part(1):
		activator.Write("\nYou find a relatively small lake, with seemingly clean water. You try tasting it, and discover that it is quite fresh and delicious. You should report your findings to Sam Goodberry...", COLOR_YELLOW)
		qm.start(2)
		qm.complete(1, sound = False)
	elif qm.started_part(3) and not qm.completed_part(3):
		activator.Write("\nIn order to fill the empty barrel with water from the lake, stand next to the lake and interact with the water barrel inside your inventory.", COLOR_YELLOW)

main()
