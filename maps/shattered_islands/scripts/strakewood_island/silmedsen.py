## @file
## Script for Silmedsen, the ancient tree near Fort Sether.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import ConstructionOfTelescope as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
    if qm.started_part(5) and not qm.started_part(6):
        if msg == "hello":
            inf.add_msg("The tree doesn't seem to notice your presence...", COLOR_YELLOW)
            inf.add_link("Can I have some of your branches?", dest = "havesome")

        elif msg == "havesome":
            inf.add_msg("The tree shifts slightly, then starts speaking in a deep voice...", COLOR_YELLOW)
            inf.add_msg("Ho-hum... No... You cannot have my branches... Unless... Unless you could bring me some refreshing water... The swamp water here is not very healthy...")
            inf.add_link("Alright...", dest = "alright")

        elif msg == "alright":
            inf.add_msg("Good... I have heard the water that surrounds the Great Blue Crystal in Morliana is clean and fresh... Bring me some in this empty potion bottle and you can have some of my branches...")
            inf.add_objects(me.FindObject(archname = "silmedsen_potion_bottle"))
            qm.start(6)

    elif qm.started_part(6) and not qm.completed_part(6) and not qm.finished(5):
        obj = activator.FindObject(archname = "silmedsen_potion_bottle_filled")

        if msg == "hello":
            inf.add_msg("Ho-hum... Do you have the potion bottle with water that surrounds the Great Blue Crystal in Morliana yet?")

            if not obj:
                inf.add_link("Not yet...", action = "close")
            else:
                inf.add_link("Here it is.", dest = "hereis")

        elif obj:
            if msg == "hereis":
                obj.Remove()
                inf.add_msg("Good... Thank you... Now I can go back to resting... Accept these branches as my gift to you...")
                inf.add_objects(me.FindObject(archname = "silmedsen_branches"))

    else:
        if msg == "hello":
            inf.add_msg("The tree doesn't seem to notice your presence...", COLOR_YELLOW)

main()
inf.finish()
