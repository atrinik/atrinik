## @file
## Script for Rienn Howell in Fort Ghzal.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import TwoLoversDoomed as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
    if qm.need_complete("deliver tortwalds letter"):
        if msg == "hello":
            inf.add_msg("{} doesn't seem to notice you and continues to stare in the direction of the ruins north...".format(me.name), COLOR_YELLOW)
            inf.add_link("Here, take this letter from your husband...", dest = "takeletter")

        elif msg == "takeletter":
            inf.add_msg("Oh! He has been gone for a long time now...")
            inf.add_msg("Thank you, adventurer... Please, take this letter and deliver it to him...")
            inf.add_objects(me.FindObject(archname = "two_lovers_doomed_letter2"))
            qm.start("deliver rienns letter")
            qm.complete("deliver tortwalds letter")

    else:
        inf.add_msg("{} doesn't seem to notice you and continues to stare in the direction of the ruins north...".format(me.name), COLOR_YELLOW)

main()
inf.finish()
