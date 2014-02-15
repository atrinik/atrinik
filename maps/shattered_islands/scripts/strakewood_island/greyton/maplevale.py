## @file
## Implements Maplevale, the mayor of Brynknot and a priest of Llwyfen.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import LlwyfenPortal as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
    def greeting():
        inf.add_msg("Greetings, {}. I am {}, a priest of the elven god Llwyfen.".format(activator.name, me.name))
        inf.add_msg("I'm also the mayor of Brynknot, and I'm currently attending to some business here in Greyton.")

    if not qm.started("portal found"):
        if msg == "hello":
            greeting()

    elif not qm.completed("portal found"):
        if msg == "hello":
            greeting()
            inf.add_link("I have found a portal sealed by the powers of Llwyfen.", dest = "sealedportal")

        elif msg == "sealedportal":
            inf.add_msg("Hmm... So you're saying you found a portal in Underground City that is sealed by the powers of the elven god Llwyfen, which I'm a priest of? That is the most unsettling news I have heard about that area recently...")
            inf.add_msg("If you like, I can give you an amulet that should break the seal, but please come back to me once you learn what's beyond that portal.")
            inf.add_link("Alright.", dest = "alright")

        elif msg == "alright":
            inf.add_msg("Very well then. Take the amulet, and go investigate what's beyond that strange portal.")
            inf.add_objects(me.FindObject(archname = "amulet_llwyfen"))
            qm.start("portal investigate")
            qm.complete("portal found")

    elif qm.need_complete("portal investigate"):
        if msg == "hello":
            inf.add_msg("I have asked you to investigate the area beyond the strange portal that you told me about in the Underground City. Have you learned anything yet?")

            if qm.finished("portal investigate"):
                inf.add_link("Yes, I found this note.", dest = "foundnote")
            else:
                inf.add_link("Not yet.", action = "close")

        elif qm.finished("portal investigate"):
            if msg == "foundnote":
                inf.add_msg("You hand over the note...", COLOR_YELLOW)
                inf.add_msg("Hmm! This note is most troubling indeed.")
                inf.add_msg("Please go see Talthor Redeye, the captain of the Brynknot guards, immediately. I'll send a runner to explain the situation to him, but he'll likely have some orders for you. You should be able to find him in his office, above the guard barracks.")
                qm.start("speak captain")
                qm.complete("portal investigate")

    elif qm.completed():
        if msg == "hello":
            inf.add_msg("I deeply appreciate you helping us get rid of that threat. If not for you, it is likely that Brynknot would have been overrun by the demons.")

    else:
        if msg == "hello":
            inf.add_msg("Time is of the essence. Please follow Talthor Redeye's orders. You should be able to find him in his office, above the guard barracks in Brynknot.")

main()
inf.finish()
