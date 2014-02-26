## @file
## Handles Brownrott the kobold, and his involvement in the 'Fort Sether
## Illness' quest.

from QuestManager import QuestManagerMulti
from Interface import InterfaceBuilderQuest
from Quests import FortSetherIllness as quest

class InterfaceDialog(InterfaceBuilderQuest):
    """
    Quest has not been completed yet, give the kobold some interesting
    dialog. This is also used during the quest.
    """

    def dialog_hello(self):
        self.add_msg("Well, hello there! I am {npc.name} the kobold. Don't you think my garden is just beautiful?")
        self.add_link("Your garden?", dest = "garden")

    def dialog_garden(self):
        self.add_msg("Why yes! Just look around you! I use a specially crafted potion that keeps my garden looking nice. Do you want to see?")
        self.add_link("Why not...", dest = "whynot")

    def dialog_whynot(self):
        self.add_msg("Here it is! Doesn't it smell wonderful?")
        self.add_msg("You start feeling nauseous as soon as the potion is opened...", color = COLOR_YELLOW)
        self.add_msg("Oh right, sorry! Let me close it again... I forgot it has ingredients that make most creatures sick...")
        self.add_msg("As the potion closes, you start feeling better again.", color = COLOR_YELLOW)

class InterfaceDialog_need_complete_figure(InterfaceDialog):
    """
    Started the quest and need to figure out the source of the illness, so
    inherit the InterfaceDialog dialog, and progress the quest at the end of
    it.
    """

    def dialog_whynot(self):
        InterfaceDialog.dialog_whynot(self)
        self.add_msg("Perhaps you should report your findings to Gwenty...", color = COLOR_YELLOW)

        self.qm.start("report")
        self.qm.complete("figure")

class InterfaceDialog_need_complete_deliver_potion(InterfaceBuilderQuest):
    """
    Have a potion for the kobold, attempt to give it to him. He will, however,
    refuse and will want the player to do a quest for him.
    """

    def dialog_hello(self):
        self.add_msg("Well, hello there again! My garden is just bea--")
        self.add_msg("You interrupt Brownrott and explain to him about the illness in Fort Sether and his potion...", color = COLOR_YELLOW)
        self.add_msg("Are you sure? Hmm... I don't know... I don't really trust anyone with my potion except myself... But perhaps... If you bring me something tasty, I might change my mind...")
        self.add_link("What do you want?", dest = "want")

    def dialog_want(self):
        self.add_msg("Spider hearts, of course, I want spider hearts - they are very tasty, but the spiders are dangerous.")
        self.add_msg("Bring me 10 sword spider hearts, and I'll mix your potion with mine. You can find those spiders around in this cave. I usually stay far away from them, but their hearts sure are delicious...")

        self.qm.start(["deliver_potion", "get_hearts"])

class InterfaceDialog_need_finish_get_hearts(InterfaceBuilderQuest):
    """
    Don't have 10 sword spider hearts yet.
    """

    def dialog_hello(self):
        self.add_msg("Bring me 10 sword spider hearts, and I'll mix your potion with mine. You can find those spiders around in this cave. I usually stay far away from them, but their hearts sure are delicious...")

class InterfaceDialog_need_complete_get_hearts(InterfaceBuilderQuest):
    """
    Brought 10 sword spider hearts.
    """

    def dialog_hello(self):
        self.add_msg("Mmm! Delicious sword spider hearts! I like you! Mmm! Alright, let's mix the potions then!")
        self.add_msg("You hand the 10 sword spider hearts and the potion to Brownrott.", color = COLOR_YELLOW)
        self.add_msg("There! All done. Thank you again for the sword spider hearts! Mmm!")
        self.add_msg("You should report the good news to Gwenty.", color = COLOR_YELLOW)

        qm.start("reward")
        qm.complete(["deliver_potion", "get_hearts"])
        qm.complete("deliver_potion")

class InterfaceDialog_completed(InterfaceBuilderQuest):
    """
    Quest has been completed.
    """

    def dialog_hello(self):
        self.add_msg("Well, hello there again! My garden is just beautiful, isn't it?")
        self.add_link("Right...", action = "close")

qm = QuestManagerMulti(activator, quest)
ib = InterfaceBuilderQuest(activator, me)
ib.finish(locals(), qm, msg)
