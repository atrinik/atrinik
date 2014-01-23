## @file
## Handles some of the guards in Fort Sether, who hint at the 'Fort Sether Illness'
## quest.

from QuestManager import QuestManagerMulti
from Quests import FortSetherIllness as quest
from Interface import Interface

qm = QuestManagerMulti(activator, quest)
inf = Interface(activator, me)

def main():
    if not qm.completed():
        if msg == "hello":
            inf.add_msg("Watch out, stranger. We seem to have an illness going on here.")
            inf.add_link("What sort of illness?", dest = "illness")
            inf.add_link("I'll keep that in mind.", action = "close")

        elif msg == "illness":
            inf.add_msg("That is part of the problem... we're not quite sure. Our local priestess, Gwenty, has been unable to find a cure for it. If you want more information, you can find her down the stairs in the main building.")

    else:
        if msg == "hello":
            inf.add_msg("I'm grateful for your help with the illness. Please, if you need anything, don't hesitate to ask.")

main()
inf.finish()
