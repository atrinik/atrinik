## @file
## Implements the quest for ring of prayers.

from Interface import InterfaceBuilderQuest
from QuestManager import QuestManager
from Quests import GallansRevenge as quest

class InterfaceDialog_need_start(InterfaceBuilderQuest):
    """
    Dialog for starting the quest.
    """

    def dialog_hello(self):
        self.add_msg("Hello there, {activator.name}. I'm {npc.name}, the old smith of Brynknot.")
        self.add_msg("If you need services, I suggest you go speak to my son, Conbran Strongfist, southwest from the Main Square. He is the owner of the family's shop now, ever since I was hurt in this fight many years ago...")
        self.add_msg("I also hear there are superb smiths in Asteria and Greyton.")
        self.add_link("Tell me more about the fight...", dest = "tellmore")

    def dialog_tellmore(self):
        self.add_msg("Ah, well, you see, many years ago, while I was still young, I used to adventure now and again. However, one day, while taking a stroll in the Giant Mountains, one of the stone giants living nearby ambushed me! I was hurt badly that day, and only just managed to survive. However, one of my arms was too badly damaged, and since then I haven't been able to do most of the work as a smith...")
        self.add_link("Tell me more about this stone giant.", dest = "tellgiant")

    def dialog_tellgiant(self):
        self.add_msg("He must have been one of the stone giants from Old Outpost, which is nearby from where I was when he ambushed me... His name was Torathog, since he kept yelling at me, saying things like 'Torathog will eat ya!'.")
        self.add_msg("In fact, I have heard that this same stone giant was seen just recently near Old Outpost, trying to ambush a party of brave adventurers. Unfortunately, the adventurers did not manage to finish off the giant, as he ran away and hid somewhere, likely in Old Outpost...")
        self.add_link("I could go and look for him, if you want.", dest = "lookfor")
        self.add_link("Goodbye.", action = "close")

    def dialog_lookfor(self):
        self.add_msg("You would? That sounds dangerous! But I would be very glad if this stone giant was brought to justice... Very well... Old Outpost is at the peak of Giant Mountains, which are north of Aris. If you manage to find Torathog, please bring him to justice.")

        self.qm.start()

class InterfaceDialog_need_finish(InterfaceBuilderQuest):
    """
    Player has not yet killed the giant, tell them where he is.
    """

    def dialog_hello(self):
        self.add_msg("Have you managed to find Torathog yet? He's likely hiding in the Old Outpost, at the peak of Giant Mountains...")
        self.add_link("Not yet.", action = "close")

class InterfaceDialog_need_complete(InterfaceBuilderQuest):
    """
    Player has killed the giant, give reward.
    """

    def dialog_hello(self):
        self.add_msg("Have you managed to find Torathog yet? He's likely hiding in the Old Outpost, at the peak of Giant Mountains...")
        self.add_link("He has been taken care of.", dest = "takencare")

    def dialog_takencare(self):
        self.add_msg("Indeed? Thank you, {activator.name}. I'm glad he has been brought to justice, after all these years.")
        self.add_msg("Here, take this ring. My days of adventuring have been over for a long time now, and I'm sure you can put the ring to better use on your journeys than I could.")
        self.add_objects(me.FindObject(archname = "ring_prayers"))

        self.qm.complete()

class InterfaceDialog_completed(InterfaceBuilderQuest):
    """
    Thank the player after completing the quest.
    """

    def dialog_hello(self):
        self.add_msg("Thank you for bringing that stone giant to justice. I can now sleep peacefully at night, knowing that the stone giant will never trouble anyone ever again...")

qm = QuestManager(activator, quest)
ib = InterfaceBuilderQuest(activator, me)
ib.finish(locals(), qm, msg)
