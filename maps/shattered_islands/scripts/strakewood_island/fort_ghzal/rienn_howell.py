## @file
## Script for Rienn Howell in Fort Ghzal.

from Interface import InterfaceBuilderQuest
from QuestManager import QuestManagerMulti
from Quests import TwoLoversDoomed as quest

class InterfaceDialog(InterfaceBuilderQuest):
    """
    Default dialog; regardless of whether the quest has been completed or not,
    Rienn just stares in the direction of the ruins.
    """

    def dialog_hello(self):
        self.add_msg("{npc.name} doesn't seem to notice you and continues to stare in the direction of the ruins north...", color = COLOR_YELLOW)

class InterfaceDialog_need_complete_deliver_tortwalds_letter(InterfaceDialog):
    """
    Dialog when the player has Tortwald's letter, and needs to deliver it to
    Rienn.
    """

    def dialog_hello(self):
        InterfaceDialog.dialog_hello(self)
        self.add_link("Here, take this letter from your husband...", dest = "takeletter")

    def dialog_takeletter(self):
        self.add_msg("Oh! He has been gone for a long time now...")
        self.add_msg("Thank you, adventurer... Please, take this letter and deliver it to him...")
        self.add_objects(me.FindObject(archname = "two_lovers_doomed_letter2"))

        self.qm.start("deliver_rienns_letter")
        self.qm.complete("deliver_tortwalds_letter")

qm = QuestManagerMulti(activator, quest)
ib = InterfaceBuilderQuest(activator, me)
ib.finish(locals(), qm, msg)
