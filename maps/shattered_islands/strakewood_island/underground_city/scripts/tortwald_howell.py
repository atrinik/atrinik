## @file
## Script for Tortwald Howell, a guard imprisoned in Underground City.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import TwoLoversDoomed as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
    if not qm.started("deliver tortwalds letter"):
        if msg == "hello":
            inf.add_msg("*sob*... Hello there...")
            inf.add_link("Who are you?", dest = "who")

        elif msg == "who":
            inf.add_msg("I'm {}, a guard from Fort Ghzal...".format(me.name))
            inf.add_link("What are you doing here?", dest = "doing")

        elif msg == "doing":
            inf.add_msg("I was captured by the creatures living in this terrible place... I don't know what they want to do with me...")
            inf.add_link("Can I help you?", dest = "help")

        elif msg == "help":
            inf.add_msg("No... I'm too weak to escape... And these bars are impervious to everything I have tried... But... Would you, please, deliver a letter to my wife in Fort Ghzal for me...?")
            inf.add_link("Sure.", dest = "sure")
            inf.add_link("No, too busy.", dest = "no")

        elif msg == "no":
            inf.add_msg("*sobs*... Please?")
            inf.add_link("Alright then.", dest = "sure")
            inf.add_link("I said no! I don't have time for your sniveling.", action = "close")

        elif msg == "sure":
            inf.add_msg("Thank you... Here's the letter. Please, deliver it to my wife, Rienn Howell in Fort Ghzal...")
            inf.add_objects(me.FindObject(archname = "two_lovers_doomed_letter1"))
            qm.start("deliver tortwalds letter")

    elif not qm.completed("deliver tortwalds letter"):
        if msg == "hello":
            inf.add_msg("*sobs*... Have you delivered the letter yet?")
            inf.add_link("Working on it.", dest = "working")

        elif msg == "working":
            inf.add_msg("Please, deliver it to my wife, Rienn Howell in Fort Ghzal...")

    elif qm.need_complete("deliver rienns letter"):
        if msg == "hello":
            inf.add_msg("*sobs*... Have you delivered the letter yet?")
            inf.add_link("Yes, take this letter from your wife.", dest = "takeletter")

        elif msg == "takeletter":
            inf.add_msg("You have a letter for me from my dear Rienn? Oh... thank you...")
            inf.add_msg("Here, take this key... it supposedly opens something around here, but before I could figure it out, I was captured... and... thank you again.")
            inf.add_objects(me.FindObject(archname = "uc_ii_skull_key"))
            qm.complete("deliver rienns letter")

    else:
        if msg == "hello":
            inf.add_msg("Thank you...")

main()
inf.finish()
