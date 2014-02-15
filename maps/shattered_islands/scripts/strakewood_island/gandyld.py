## @file
## Quest for Gandyld, an old mage living east of Aris.

from Interface import Interface
from QuestManager import QuestManagerMulti
from Quests import GandyldsManaCrystal as quest

inf = Interface(activator, me)
qm = QuestManagerMulti(activator, quest)

def main():
    if not qm.started("enhance crystal alchemists"):
        if msg == "hello":
            inf.add_msg("{} mumbles something and slowly turns his attention to you.".format(me.name), COLOR_YELLOW)
            inf.add_msg("What is it? Can't you see I'm busy here?")
            inf.add_link("What are you so busy with?", dest = "busy")
            inf.add_link("Sorry, I'll be on my way.", action = "close")

        elif msg == "busy":
            inf.add_msg("Hmph. I'm in the process of creating a very powerful mana crystal. Now, please excuse me. Like I said, I'm busy.")
            inf.add_link("Tell me more...", dest = "tellmore")
            inf.add_link("Sorry, I'll be on my way.", action = "close")

        elif msg == "tellmore":
            inf.add_msg("You just don't want to give up, do you? Okay, I will tell you about mana crystals...")
            inf.add_msg("Mana crystals are items sought after by mages. They allow you to store a certain amount of mana, you see.")
            inf.add_link("How does that work?", dest = "howwork")

        elif msg == "howwork":
            inf.add_msg("When a mage applies a mana crystal while he is full on mana, half of his mana will be transferred to the crystal. The mage can then apply the crystal at any time to get the mana back. Crystals have a maximum mana capacity, so mages are always after the ones that can hold the most.")
            inf.add_link("Where can I get one?", dest = "whereget")

        elif msg == "whereget":
            inf.add_msg("They're quite rare, but some merchants trade them occasionally, or so I hear.")
            inf.add_msg("Hm, I seem to have a crystal I don't need right here. Do you want it?")
            inf.add_link("Yes, please!", dest = "yespls")

        elif msg == "yespls":
            inf.add_msg("Very well then... Here you go, and take care of it. It's not powerful, but it could still be useful to you.")
            inf.add_objects(me.FindObject(archname = "gandyld_crystal_1"))
            inf.add_msg("Now, listen. I have heard that King Rhun's alchemists are trying to come up with a concoction that would enhance the mana limit of weak crystals, like the one I have just given you. It is strange to see giants coming up with magical enchantments, so it may not even work, or maybe explode on contact with the crystal. However, feel free to go and check it out, but if you do and it works, please come back so I can examine the crystal.")
            inf.add_msg("The heart of the Giant Mountains, just north of here, is King Rhun's domain. He has an outpost of sorts at the very peak of the mountains, where it is so cold most don't even dare approach. The concoction is likely to be in a cauldron or a pool in their laboratory. Be careful.")
            qm.start("enhance crystal alchemists")

    elif not qm.completed("enhance crystal alchemists"):
        if msg == "hello":
            inf.add_msg("So, how did it go? Have you found the concoction and managed to enhance the crystal I have given you?")

            if not qm.finished("enhance crystal alchemists"):
                inf.add_link("Working on it...", action = "close")
            else:
                inf.add_link("Yes, it worked. See for yourself...", dest = "worked")

        elif qm.finished("enhance crystal alchemists"):
            if msg == "worked":
                inf.add_msg("Hmm, I see! That is very interesting... and rather odd. You can still smell the brimstone too...")
                inf.add_msg("Very well... Seeing that their enchantment works, it is rather worrying. Maybe the giants are becoming intelligent? Hm... King Rhun is rather intelligent for a giant, and likes to experiment with alchemy and some magic, but most other giants? Very strange indeed.")
                inf.add_msg("At any rate, this seems like the sort of thing King Rhun would be interested in experimenting with, making it better. If you can find his concoction, you might be able to improve the crystal I have given you even further. I suggest trying to look for his vault or similar.")
                qm.start("enhance crystal rhun")
                qm.complete("enhance crystal alchemists")

    elif not qm.completed("enhance crystal rhun"):
        if msg == "hello":
            inf.add_msg("So, how did it go? Have you found the concoction and managed to further enhance the crystal I have given you?")

            if not qm.finished("enhance crystal rhun"):
                inf.add_link("Working on it...", action = "close")
            else:
                inf.add_link("Yes, I found it and it worked. See for yourself...", dest = "worked")

        elif qm.finished("enhance crystal rhun"):
            if msg == "worked":
                inf.add_msg("Well, well... very interesting, {}. Thank you for the information, it's likely going to be quite valuable in my own research.".format(activator.name))
                inf.add_objects(me.FindObject(archname = "silvercoin"))
                inf.add_msg("Use that enhanced mana crystal well.")
                qm.complete("enhance crystal rhun")

    else:
        if msg == "hello":
            inf.add_msg("Welcome back, {}.".format(activator.name))
            inf.add_msg("Thanks to your information about the concoction, my research is going well.")

main()
inf.finish()
