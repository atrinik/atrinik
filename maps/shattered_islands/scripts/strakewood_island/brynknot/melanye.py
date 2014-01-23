## @file
## Quest to recover an old woman's walking stick, which was stolen from her
## in the middle of the night at Brynknot Tavern.

from Interface import Interface
from QuestManager import QuestManager
from Quests import MelanyesLostWalkingStick as quest

inf = Interface(activator, me)
qm = QuestManager(activator, quest)

def main():
    if not qm.started():
        if msg == "hello":
            inf.add_msg("Well, hello there my dear. Say -- have you seen any evil treants around the place?")
            inf.add_link("Why are you asking?", dest = "whyasking")

        elif msg == "whyasking":
            inf.add_msg("I was asleep just last night, when a treant broke into my room here at the tavern, through the window!")
            inf.add_link("How did you know it was a treant?", dest = "howknow")

        elif msg == "howknow":
            inf.add_msg("Well, it woke me up! I only saw an evil-looking treant running away, holding my enchanted walking stick!")
            inf.add_link("Your enchanted walking stick?", dest = "walkingstick")

        elif msg == "walkingstick":
            inf.add_msg("My walking stick has been enchanted to give off a soft glow wherever it goes. That is how I saw that it was a treant, running off to the east. I'm not sure what use a walking stick is to a treant, but maybe it just liked the soft glow... But now I have to use an ordinary walking stick... Say, would you look for this treant and recover my walking stick? I might have something in return for your troubles...")
            inf.add_link("Sure...", dest = "sure")

        elif msg == "sure":
            inf.add_msg("Why, thank you! I'm sure a strong-looking adventurer like yourself will be able to sort this out in no time at all. Just head east of Brynknot -- that is where the treant ran off to...")
            qm.start()

    elif not qm.completed():
        if msg == "hello":
            inf.add_msg("Have you recovered my walking stick yet? I'm sure the evil treant ran off to the east of Brynknot...")

            if not qm.finished():
                inf.add_link("Working on it.", action = "close")
            else:
                inf.add_link("Here it is.", dest = "hereis")

        elif qm.finished():
            if msg == "hereis":
                inf.add_msg("Oh, you found it! Thank you, thank you...")
                inf.add_msg("Here, it's all I can spare... I hope it will be useful to you.")
                inf.add_objects(me.FindObject(archname = "silvercoin"))
                qm.complete()

    else:
        if msg == "hello":
            inf.add_msg("Thank you for recovering my walking stick and teaching that evil treant a lesson!")

main()
inf.finish()
