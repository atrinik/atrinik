## @file
## Script for the researcher Albar in Morliana.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import ConstructionOfTelescope as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
    if not qm.started_part(1):
        if msg == "hello":
            inf.add_msg("Well, hello there! Isn't this experiment simply fascinating?")
            inf.add_link("Tell me more about the experiment", dest = "experiment")

        elif msg == "experiment":
            inf.add_msg("You don't know about it? We are trying to figure out the exact properties of this shard, which came from the Great Blue Crystal. But I assume you know all about that already.")
            inf.add_link("How's it going?", dest = "going")

        elif msg == "going":
            inf.add_msg("Well... not very well, actually. We have been looking for more shards of the Great Blue Crystal, but we can't find any. There have been reports about some blue crystal shards in Brynknot, but we have been unable to locate any there. However, it sounds plausible, as there was huge earthquake in Brynknot recently, just like the one that happened on this island when the Great Blue Crystal fell here... Hm...")
            inf.add_link("Could I help you?", dest = "help")
            inf.add_link("Goodbye.", action = "close")

        elif msg == "help":
            inf.add_msg("Hm... Sorry? Oh, do you really want to help me? Very well then. Please go to Brynknot and find someone who collected a shard of the crystal, and bring it back to me. The owner will probably be a mage or alchemist. I heard there was an alchemist in Brynknot, it could be him...")
            qm.start(1)

    elif not qm.completed_part(1):
        if msg == "hello":
            inf.add_msg("Have you found the crystal shard yet?")

            if not qm.finished(1):
                inf.add_link("Working on it...", action = "close")
            else:
                inf.add_link("Yes, here you go.", dest = "herego")

        elif qm.finished(1):
            if msg == "herego":
                inf.add_msg("You hand the blue crystal fragment to {} and tell him about the earthquake and the investigation...".format(me.name), COLOR_YELLOW)
                inf.add_msg("Thank you {}. That is indeed very interesting report you have from Jonaslen... And the fragment! My piece and this fragment look very much alike. It is odd... My piece came from the sky, and the fragment came from the ground?... Hmmm... This is very confusing indeed. Please, go back to Brynknot and ask Jonaslen whether there were reports of a flash in the sky just before the earthquake.".format(activator.name))
                qm.start(2)
                qm.complete(1, sound = None)

    elif qm.started_part(2) and not qm.started_part(4):
        if msg == "hello":
            inf.add_msg("Did you ask Jonaslen whether there were reports of a flash in the sky just before the earthquake?")

            if not qm.completed_part(2):
                inf.add_link("Working on it...", action = "close")
            else:
                inf.add_link("Yes...", dest = "yes")

        elif qm.completed_part(2):
            if msg == "yes":
                inf.add_msg("That makes sense... Quite interesting, it seems the crystal that shattered in Brynknot was the same as the one here in Morliana, the Great Blue Crystal, and both fell from the sky... Well, I need to construct a telescope so I can study the sky to see if there are any more crystals we should know about. But I need some special glass lens crystal first... Would you get it for me, please?")
                inf.add_link("Sure.", dest = "sure")

            elif msg == "sure":
                inf.add_msg("Great! I have heard Morg'eean the kobold trader south of Asteria trades clear crystals, you should be able to find one in his little shop.")
                qm.start(4)
                qm.complete(3, sound = None)

    elif qm.started_part(4) and not qm.started_part(5):
        obj = activator.FindObject(archname = "jewel_generic", name = "clear crystal")

        if msg == "hello":
            inf.add_msg("Have you got the clear crystal from Morg'eean the kobold trader south of Asteria yet?")

            if not obj:
                inf.add_link("Not yet...", action = "close")
            else:
                inf.add_link("Yes, here you go.", dest = "herego")

        elif obj:
            if msg == "herego":
                obj.Decrease()
                inf.add_msg("You hand one clear crystal to {}.".format(me.name), COLOR_YELLOW)
                inf.add_msg("That's a perfect clear crystal, thank you! Now, I need a stand to mount the telescope on. It needs to be a very sturdy one... The wood from the ancient tree Silmedsen should do. I have heard he was located south of Asteria Swamp, near Fort Sether...")
                qm.start(5)
                qm.complete(4, sound = False)

    elif qm.started_part(5) and not qm.completed_part(5):
        if msg == "hello":
            inf.add_msg("Have you found the wood from the ancient tree Silmedsen yet? I have heard he was located south of Asteria Swamp, near Fort Sether...")

            if not qm.finished(5):
                inf.add_link("Working on it...", action = "close")
            else:
                inf.add_link("Yes, here you go.", dest = "herego")

        elif qm.finished(5):
            if msg == "herego":
                inf.add_msg("That's great wood, just perfect, thank you. Now I can finish the telescope... Please, accept this gift from me. It's some protection against the winter here in the cold North.")
                inf.add_objects(list(me.FindObject(archname = "sack").inv))
                qm.complete()

    else:
        inf.add_msg("The telescope is now finished and I can study the sky, all thanks to you, my friend!")

main()
inf.finish()
