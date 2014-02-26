## @file
## Script for Tortwald Howell, a guard imprisoned in Underground City.

from Interface import InterfaceBuilderQuest
from QuestManager import QuestManagerMulti
from Quests import TwoLoversDoomed as quest

class InterfaceDialog_need_start_deliver_tortwalds_letter(InterfaceBuilderQuest):
    """
    Dialog for starting Tortwald's letter delivery quest.
    """

    def dialog_hello(self):
        self.add_msg("*sob*... Hello there...")
        self.add_link("Who are you?", dest = "who")

    def dialog_who(self):
        self.add_msg("I'm {npc.name}, a guard from Fort Ghzal...")
        self.add_link("What are you doing here?", dest = "doing")

    def dialog_doing(self):
        self.add_msg("I was captured by the creatures living in this terrible place... I don't know what they want to do with me...")
        self.add_link("Can I help you?", dest = "help")

    def dialog_help(self):
        self.add_msg("No... I'm too weak to escape... And these bars are impervious to everything I have tried... But... Would you, please, deliver a letter to my wife in Fort Ghzal for me...?")
        self.add_link("Sure.", dest = "sure")
        self.add_link("No, too busy.", dest = "no")

    def dialog_no(self):
        self.add_msg("*sobs*... Please?")
        self.add_link("Alright then.", dest = "sure")
        self.add_link("I said no! I don't have time for your sniveling.", action = "close")

    def dialog_sure(self):
        self.add_msg("Thank you... Here's the letter. Please, deliver it to my wife, Rienn Howell in Fort Ghzal...")
        self.add_objects(me.FindObject(archname = "two_lovers_doomed_letter1"))

        self.qm.start("deliver_tortwalds_letter")

class InterfaceDialog_need_complete_deliver_tortwalds_letter(InterfaceBuilderQuest):
    """
    Dialog to remind the player about delivering the letter.
    """

    def dialog_hello(self):
        self.add_msg("*sobs*... Have you delivered the letter yet?")
        self.add_link("Working on it.", dest = "working")

    def dialog_working(self):
        self.add_msg("Please, deliver it to my wife, Rienn Howell in Fort Ghzal...")

class InterfaceDialog_need_complete_deliver_rienns_letter(InterfaceBuilderQuest):
    """
    Dialog when the player has delivered the letter, and has Rienn's to deliver.
    """

    def dialog_hello(self):
        self.add_msg("*sobs*... Have you delivered the letter yet?")
        self.add_link("Yes, take this letter from your wife.", dest = "takeletter")

    def dialog_takeletter(self):
        self.add_msg("You have a letter for me from my dear Rienn? Oh... thank you...")
        self.add_msg("Here, take this key... it supposedly opens something around here, but before I could figure it out, I was captured... and... thank you again.")
        self.add_objects(me.FindObject(archname = "uc_ii_skull_key"))

        self.qm.complete("deliver_rienns_letter")

class InterfaceDialog_completed(InterfaceBuilderQuest):
    """
    Quest has been completed.
    """

    def dialog_hello(self):
        self.add_msg("Thank you...")

qm = QuestManagerMulti(activator, quest)
ib = InterfaceBuilderQuest(activator, me)
ib.finish(locals(), qm, msg)
