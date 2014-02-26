## @file
## Handles Gwenty, a priestess of Grunhilde, located in Fort Sether.
##
## Gives out the 'Fort Sether Illness' quest.

from QuestManager import QuestManagerMulti
from Interface import InterfaceBuilderQuest
from Quests import FortSetherIllness as quest
import Temple

class InterfaceDialog_need_start_figure(InterfaceBuilderQuest):
    """
    Player has not yet agreed to figuring out the cause of the sickness.
    """

    def dialog_hello(self):
        self.add_msg("Welcome, stranger. I'd like to help you by offering my usual priest services, however, the illness that is going on in the fort is keeping me rather busy, so if you'll excuse me...")
        self.add_link("What sort of illness?", dest = "illness")

    def dialog_illness(self):
        self.add_msg("Well, if you're so curious, I can spare a few moments. Perhaps you might be able to help us...")
        self.add_msg("Many guards are falling ill, one after another. Being the only priestess around, I'm quite busy tending the sick guards. However, I'm not able to completely cure them, as they just keep falling ill soon after I cure them...")
        self.add_link("Do you know the reason?", dest = "reason")

    def dialog_reason(self):
        self.add_msg("If only! If I wasn't so busy, I would probably be able to figure it out. But as it is...")
        self.add_link("Could I help?", dest = "help")
        self.add_link("I see...", dest = "see")

    def dialog_see(self):
        self.add_msg("Would you be willing to help us? I am afraid I can't solve this problem without some assistance...")
        self.add_link("Sure, I'll help.", dest = "help")
        self.add_link("Not interested.", action = "close")

    def dialog_help(self):
        self.add_msg("Ah, yes! Thank you. Your help is much appreciated...")
        self.add_msg("I'm not sure where the disease is originating from. The only thing I can think of is the water, which we get from an underground river of sorts.")
        self.add_msg("It would certainly be worth investigating the river... perhaps something is going on down there. The fastest way to do that would be to climb down using one of the water wells in the fort.")

        self.qm.start("figure")

class InterfaceDialog_need_complete_figure(InterfaceBuilderQuest):
    """
    Player needs to figure out the source of the sickness.
    """

    def dialog_hello(self):
        self.add_msg("You agreed to help us {activator.name}, no?")
        self.add_msg("I still suspect the cause of the illness is the water, which we get from an underground river of sorts.")
        self.add_msg("It would certainly be worth investigating the river... perhaps something is going on down there. The fastest way to do that would be to climb down using one of the water wells in the fort.")

class InterfaceDialog_need_complete_report(InterfaceBuilderQuest):
    """
    Reporting about Brownrott's potion.
    """

    def dialog_hello(self):
        self.add_msg("Well? Have you investigated the water wells yet?")
        self.add_link("Yes...", dest = "yes")

    def dialog_yes(self):
        self.add_msg("You explain about Brownrott and his garden...", color = COLOR_YELLOW)
        self.add_msg("Indeed? So it was the water, like I thought... Well, I think I know how to mend this particular problem...")
        self.add_objects(me.FindObject(name = "Gwenty's Potion"))
        self.add_msg("Here. Take that potion, and bring it to the kobold. Tell him he needs to mix it with his own. It should cancel out the negative effects it causes to us humans, but still make it effective for his garden. It might take some persuading, however...")

        self.qm.start("deliver_potion")
        self.qm.complete("report")

class InterfaceDialog_need_complete_deliver_potion(InterfaceBuilderQuest):
    """
    Still need to deliver the potion to Brownrott.
    """

    def dialog_hello(self):
        self.add_msg("So, have you convinced the kobold to mix his potion with the one I gave you?")
        self.add_link("Still working on it.", dest = "working")

    def dialog_working(self):
        self.add_msg("Very well...")

class InterfaceDialog_need_finish_deliver_potion(
    InterfaceDialog_need_complete_deliver_potion
):
    """
    Still need to deliver the potion to Brownrott, but lost the potion.
    """

    def dialog_hello(self):
        InterfaceDialog_need_complete_deliver_potion.dialog_hello(self)
        self.add_link("I lost the potion you gave me...", dest = "lost")

    def dialog_lost(self):
        self.add_msg("What?! Well, it's not irreplaceable, but please, don't go wasting it and just convince the kobold to mix it with his potion... Here, take this spare one I have, and be more careful with it...")
        self.add_objects(me.FindObject(name = "Gwenty's Potion"))


class InterfaceDialog_need_complete_reward(InterfaceBuilderQuest):
    """
    Potion has been delivered to Brownrott, claim the reward.
    """

    def dialog_hello(self):
        self.add_msg("So, have you convinced the kobold to mix his potion with the one I gave you?")
        self.add_link("Yes, I have.", dest = "yes")

    def dialog_yes(self):
        self.add_msg("That's great news, {activator.name}! I knew I could count on you. You have my thanks, as well as that of all the people in the fort. We can now drink the water safely again...")
        self.add_msg("As for your reward... Please, accept these coins from me. Also, since I'm no longer busy tending all the sick guards, I can now resume offering you my regular services.")
        self.add_objects(me.FindObject(archname = "silvercoin"))

        self.qm.complete("reward")

class InterfaceDialog_completed(InterfaceBuilderQuest):
    """
    Quest has been completed, offer usual temple services.
    """

    def __init__(self, *args, **kwargs):
        super(InterfaceBuilderQuest, self).__init__(*args, **kwargs)
        self.temple = Temple.TempleGrunhilde(activator, me, self)

    def dialog_hello(self):
        self.temple.hello_msg = "We won't forget what you have done for us, {}.".format(activator.name)
        self.temple.handle_chat(msg)

    def dialog(self, msg):
        self.temple.handle_chat(msg)

qm = QuestManagerMulti(activator, quest)
ib = InterfaceBuilderQuest(activator, me)
ib.finish(locals(), qm, msg)
