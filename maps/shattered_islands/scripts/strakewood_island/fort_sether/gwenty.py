## @file
## Handles Gwenty, a priestess of Grunhilde, located in Fort Sether.
##
## Gives out the 'Fort Sether Illness' quest.

from QuestManager import QuestManagerMulti
from Quests import FortSetherIllness as quest
from Interface import Interface

qm = QuestManagerMulti(activator, quest)
inf = Interface(activator, me)

def main():
    # Completed the quest, offer normal temple services.
    if qm.completed():
        import Temple

        temple = Temple.TempleGrunhilde(activator, me, inf)
        temple.hello_msg = "We won't forget what you have done for us, {}.".format(activator.name)
        temple.handle_chat(msg)
    # Not started the quest, try offering the quest.
    elif not qm.started("figure"):
        if msg == "hello":
            inf.add_msg("Welcome, stranger. I'd like to help you by offering my usual priest services, however, the illness that is going on in the fort is keeping me rather busy, so if you'll excuse me...")
            inf.add_link("What sort of illness?", dest = "illness")

        elif msg == "illness":
            inf.add_msg("Well, if you're so curious, I can spare a few moments. Perhaps you might be able to help us...")
            inf.add_msg("Many guards are falling ill, one after another. Being the only priestess around, I'm quite busy tending the sick guards. However, I'm not able to completely cure them, as they just keep falling ill soon after I cure them...")
            inf.add_link("Do you know the reason?", dest = "reason")

        elif msg == "reason":
            inf.add_msg("If only! If I wasn't so busy, I would probably be able to figure it out. But as it is...")
            inf.add_link("Could I help?", dest = "help")
            inf.add_link("I see...", dest = "see")

        elif msg == "see":
            inf.add_msg("Would you be willing to help us? I am afraid I can't solve this problem without some assistance...")
            inf.add_link("Sure, I'll help.", dest = "help")
            inf.add_link("Not interested.", action = "close")

        elif msg == "help":
            inf.add_msg("Ah, yes! Thank you. Your help is much appreciated...")
            inf.add_msg("I'm not sure where the disease is originating from. The only thing I can think of is the water, which we get from an underground river of sorts.")
            inf.add_msg("It would certainly be worth investigating the river... perhaps something is going on down there. The fastest way to do that would be to climb down using one of the water wells in the fort.")
            qm.start("figure")
    # Accepted the quest, but haven't met the kobold yet.
    elif qm.need_complete("figure"):
        if msg == "hello":
            inf.add_msg("You agreed to help us {}, no?".format(activator.name))
            inf.add_msg("I still suspect the cause of the illness is the water, which we get from an underground river of sorts.")
            inf.add_msg("It would certainly be worth investigating the river... perhaps something is going on down there. The fastest way to do that would be to climb down using one of the water wells in the fort.")
    # Met the kobold.
    elif qm.need_complete("ask advice"):
        if msg == "hello":
            inf.add_msg("Well? Have you investigated the water wells yet?")
            inf.add_link("Yes...", dest = "yes")

        elif msg == "yes":
            inf.add_msg("You explain about Brownrott and his garden...", COLOR_YELLOW)
            inf.add_msg("Indeed? So it was the water, like I thought... Well, I think I know how to mend this particular problem...")
            inf.add_objects(me.FindObject(name = "Gwenty's Potion"))
            inf.add_msg("Here. Take that potion, and bring it to the kobold. Tell him he needs to mix it with his own. It should cancel out the negative effects it causes to us humans, but still make it effective for his garden. It might take some persuading, however...")
            qm.start("deliver potion")
            qm.complete("ask advice")
    # Got the potion from Gwenty, but have not convinced the kobold yet.
    elif qm.need_complete("deliver potion"):
        if msg == "hello":
            inf.add_msg("So, have you convinced the kobold to mix his potion with the one I gave you?")
            inf.add_link("Still working on it.", dest = "working")

            # If the player lost the potion, make it possible to get it back.
            if not activator.FindObject(mode = INVENTORY_CONTAINERS, name = "Gwenty's Potion"):
                inf.add_link("I lost the potion you gave me...", dest = "lost")

        elif msg == "working":
            inf.add_msg("Very well...")

        elif msg == "lost" and not activator.FindObject(mode = INVENTORY_CONTAINERS, name = "Gwenty's Potion"):
            inf.add_msg("What?! Well, it's not irreplaceable, but please, don't go wasting it and just convince the kobold to mix it with his potion... Here, take this spare one I have, and be more careful with it...")
            inf.add_objects(me.FindObject(name = "Gwenty's Potion"))
    # Convinced the kobold to mix the two potions.
    elif qm.need_complete("reward"):
        if msg == "hello":
            inf.add_msg("So, have you convinced the kobold to mix his potion with the one I gave you?")
            inf.add_link("Yes, I have.", dest = "yes")

        elif msg == "yes":
            inf.add_msg("That's great news, {}! I knew I could count on you. You have my thanks, as well as that of all the people in the fort. We can now drink the water safely again...".format(activator.name))
            inf.add_msg("As for your reward... Please, accept these coins from me. Also, since I'm no longer busy tending all the sick guards, I can now resume offering you my regular services.")
            inf.add_objects(me.FindObject(archname = "silvercoin"))
            qm.complete("reward")

main()
inf.finish()
