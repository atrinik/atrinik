## @file
## Implements the "Lairwenn's Notes" quest, which allows players to
## acquire a writing pen.

from Interface import Interface
from QuestManager import QuestManager
from Quests import LairwennsNotes as quest

inf = Interface(activator, me)
qm = QuestManager(activator, quest)

def main():
    if not qm.started():
        if msg == "hello":
            inf.add_msg("Welcome to the Brynknot Library... I'm {}, the librarian.".format(me.name))
            inf.add_msg("Please excuse me, I seem to have lost my notes somewhere...")
            inf.add_link("You lost your notes?", dest = "lostnotes")

        elif msg == "lostnotes":
            inf.add_msg("I hurriedly put them away while searching for this book, and then a while later I remembered about the notes but I can't find them now! And I really need them to finish this document I'm working on...")
            inf.add_link("Can I help you in your search?", dest = "helpsearch")

        elif msg == "helpsearch":
            inf.add_msg("I would certainly appreciate it my dear! The notes are somewhere in this library, I just can't seem to find them...")
            qm.start()

    elif qm.completed():
        if msg == "hello":
            pen = me.FindObject(name = "writing pen")
            inf.add_msg("Thank you for finding my notes earlier. Would you like to buy an extra writing pen for mere {}?".format(CostString(pen.value)))
            inf.add_link("Sure, why not.", dest = "buypen")
            inf.add_link("No thanks.", action = "close")

        elif msg == "buypen":
            pen = me.FindObject(name = "writing pen")

            if activator.PayAmount(pen.value):
                inf.add_msg("You pay {}.".format(CostString(pen.value)), COLOR_YELLOW)
                inf.add_msg("Here you go dear, a top-quality writing pen!")
                inf.add_objects(pen)
            else:
                inf.add_msg("Oh dear, it seems you don't have enough money!")

    elif qm.finished():
        if msg == "hello":
            inf.add_msg("You found my notes!")
            inf.add_msg("...")
            inf.add_msg("They were in the luggage on the top floor? How curious... Well, thank you my dear! I think I have something here that may be useful on your adventures... Now, where did I put it...")
            inf.add_msg("{} thinks for a moment.".format(me.name), COLOR_YELLOW)
            inf.add_msg("Just pulling your leg my dear! Here it is, a writing pen will be useful, will it not?")
            inf.add_objects(me.FindObject(name = "writing pen"))
            qm.complete()

    else:
        if msg == "hello":
            inf.add_msg("{} sighs.".format(me.name), COLOR_YELLOW)
            inf.add_msg("I'm trying to remember where I put them, but I just can't... Please continue your search my dear, they should be somewhere in this library...")

main()
inf.finish()
