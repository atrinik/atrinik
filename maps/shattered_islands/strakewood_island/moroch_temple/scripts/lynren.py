## @file
## Implements Lynren's quest.

from Interface import Interface
from QuestManager import QuestManager
from Quests import RescuingLynren as quest

inf = Interface(activator, me)
qm = QuestManager(activator, quest)

def main():
    if not qm.started():
        if msg == "hello":
            inf.add_msg("Hello there...")
            inf.add_link("Are you Lynren?", dest = "areyou")

        elif msg == "areyou":
            inf.add_msg("{} sighs heavily.".format(me.name), COLOR_YELLOW)
            inf.add_msg("Yes, that is my name... Lynren the paladin...")
            inf.add_link("Why are you here?", dest = "whyhere")

        elif msg == "whyhere":
            inf.add_msg("I have set out on a mission to destroy all evil, and set this evil place as my first target... But I was overpowered and my magics were subdued by the evil magicians here...")
            inf.add_link("Is there anything I can do to help?", dest = "dohelp")
            inf.add_link("I see. Farewell.", dest = "farewell")

        elif msg == "dohelp":
            inf.add_msg("Well... You could visit my home in Asteria among the various temples, and bring me my book of holy word... The holy word prayer should work to set me free... Would you do this for me?")
            inf.add_link("I will.", dest = "iwill")
            inf.add_link("Not interested.", dest = "farewell")

        elif msg == "iwill":
            inf.add_msg("Excellent! Please make haste and bring me my book of holy word from my home.")
            qm.start()

        elif msg == "farewell":
            inf.add_msg("{} sobs quietly to herself...".format(me.name), COLOR_YELLOW)

    else:
        if msg == "hello":
            inf.add_msg("Have you got my book of holy word yet? You can find the book at my home in Asteria, among the various temples... Please make haste.")

            if not qm.finished():
                inf.add_link("I'll work on that.", action = "close")
            else:
                inf.add_link("Yes, here you go.", dest = "herego")

        elif qm.finished():
            if msg == "herego":
                inf.add_msg("Splendid! Alright, let me have a look that book...")
                inf.add_msg("Normally, holy word is used to cause mass damage to undead creatures. However, this book contains a variation of the prayer, with a ritual, which allows one to break even the darkest magics.")
                inf.add_msg("This is what you have to do...")
                inf.add_link("<perform the ritual>", dest = "perform")

            elif msg == "perform":
                # Activate the spell effect.
                beacon = me.map.LocateBeacon("lynren_lever")
                beacon.env.Apply(beacon.env, APPLY_TOGGLE)

                inf.dialog_close()
                pl.DrawInfo("You use the variation of the holy word prayer to free Lynren the paladin from her imprisonment.", COLOR_GREEN)
                pl.DrawInfo("{} says: Thank you, thank you! I hope to meet you again once more... For now, farewell!".format(me.name), COLOR_NAVY)
                obj = me.FindObject(archname = "goldcoin").Clone()
                pl.DrawInfo("Moments after Lynren vanishes, some sparkling gold coins materialize in your hand out of nowhere...")
                obj.InsertInto(activator)
                qm.complete()


if not qm.completed():
    main()
    inf.finish()
