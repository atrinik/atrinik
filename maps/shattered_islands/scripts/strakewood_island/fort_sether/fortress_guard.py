## @file
## Handles some of the guards in Fort Sether, who hint at the 'Fort Sether
## Illness' quest.

from QuestManager import QuestManagerMulti
from Interface import InterfaceBuilderQuest
from Quests import FortSetherIllness as quest

class InterfaceDialog_completed(InterfaceBuilderQuest):
    """
    Quest has been completed, thank the player.
    """

    def dialog_hello(self):
        self.add_msg("I'm grateful for your help with the illness. Please, "
                     "if you need anything, don't hesitate to ask.")

class InterfaceDialog(InterfaceBuilderQuest):
    """
    Quest has not been completed yet, inform the player about it.
    """

    def dialog_hello(self):
        self.add_msg("Watch out, stranger. We seem to have an illness going "
                     "on here.")
        self.add_link("What sort of illness?", dest = "illness")
        self.add_link("I'll keep that in mind.", action = "close")

    def dialog_illness(self):
        self.add_msg("That is part of the problem... we're not quite sure. "
                     "Our local priestess, Gwenty, has been unable to find a "
                     "cure for it. If you want more information, you can find "
                     "her down the stairs in the main building.")

qm = QuestManagerMulti(activator, quest)
ib = InterfaceBuilderQuest(activator, me)
ib.finish(locals(), qm, msg)
