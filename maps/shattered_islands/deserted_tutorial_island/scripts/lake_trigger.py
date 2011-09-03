from Atrinik import *
from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest

activator = WhoIsActivator()
qm = QuestManagerMulti(activator, quest)

def main():
	SetReturnValue(1)

	if qm.started_part(1) and not qm.completed_part(1):
		activator.Write("You find a relatively small lake, with seemingly clean water. You try tasting it, and discover that it is quite fresh and delicious. You should report your findings to Sam Goodberry...", COLOR_YELLOW)
		activator.Write("You can check your quest list at any time by pressing <b>Q</b>.", "FDD017")
		qm.start(2)
		qm.complete(1, sound = False)
	elif qm.started_part(3) and not qm.completed_part(3):
		activator.Write("In order to fill the empty barrel with water from the lake, stand next to the lake, open your inventory using <b>Shift</b>, select the empty barrel using the arrow keys or the mouse and press <b>A</b> (apply).", "FDD017")

main()
