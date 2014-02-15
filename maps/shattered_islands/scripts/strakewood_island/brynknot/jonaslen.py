## @file
## Script for Jonaslen the mage in Brynknot.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import ConstructionOfTelescope as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
    if qm.need_finish("get shard"):
        if msg == "hello":
            inf.add_msg("Hello there. I am {}. I'm sorry, but I'm rather busy at the moment, so please excuse me...".format(me.name))
            inf.add_link("Have you seen any blue crystal shards around?", dest = "shards")

        elif msg == "shards":
            inf.add_msg("Why yes, I have. The recent earthquake opened up a huge hole north of Brynknot, and there were blue shards all around the place. I collected most of them for my own research. But why are you asking this?")
            inf.add_link("Albar from Morliana Research Center sent me to look for a blue crystal shard.", dest = "lookfor")

        elif msg == "lookfor":
            inf.add_msg("I see. I have heard of the Morliana Research Center. Hm... Very well then. Go back to Albar and tell him that the day the earthquake happened, we went to investigate the hole. Inside was a crystal charged with a tremendous amount of energy, with which it seemed to be protecting itself. We decided to investigate the hole more, but when we came back to look at the crystal, it was shattered completely, and only a few fragments were left. Here, take this fragment and bring it safely to Albar.")
            inf.add_objects(me.FindObject(archname = "blue_crystal_fragment"))

    elif qm.need_complete("ask about flash"):
        if msg == "hello":
            inf.add_msg("Hello again, {}. I'm still rather busy, so please excuse me...".format(activator.name))
            inf.add_link("Have there been reports about a flash in the sky just before the earthquake?", dest = "reports")

        elif msg == "reports":
            inf.add_msg("Hm... Why yes... Many of the townsfolk, including the guards on duty that day, swore they saw a flash in the sky just before the earthquake... In fact, I think it's possible the crystal that shattered here fell from the sky, which caused the earthquake and the hole. Perhaps you should take this information to Albar, it might help him.")
            qm.start("report about flash")
            qm.complete("ask about flash")

    else:
        if msg == "hello":
            inf.add_msg("Hello there. I am {}. I'm sorry, but I'm rather busy at the moment, so please excuse me...".format(me.name))

main()
inf.finish()
