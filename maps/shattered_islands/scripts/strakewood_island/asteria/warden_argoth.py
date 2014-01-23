## @file
## Implements the angry drunk Warden who drops Asteria Dungeon Key 1.

from Interface import Interface

inf = Interface(activator, me)

def main():
    if me.enemy:
        if me.enemy == activator:
            me.Say("Now you die!")
        else:
            inf.dialog_close()

    elif msg == "hello":
        inf.add_msg("{} yawns sleepily.".format(me.name), COLOR_YELLOW)
        inf.add_msg("Hello {}, I am {}. So what brings you here?".format(activator.name, me.name))
        inf.add_link("Just exploring around...", dest = "exploring")

    elif msg == "exploring":
        inf.add_msg("*HIC* I see... I don't suppose you've come about the key. Sorry, I can't give that to you...")
        inf.add_link("What key?", dest = "key")
        inf.add_link("Have you been drinking?", dest = "drinking")

    elif msg == "key":
        inf.add_msg("I told you, can't give it to you. Don't even ask about it.")
        inf.add_link("Have you been drinking?", dest = "drinking")

    elif msg == "drinking":
        inf.add_msg("... of course not, I'm on duty. What makes you think that?")
        inf.add_link("You drunkard.", dest = "drunkard1")

    elif msg == "drunkard1":
        inf.add_msg("What did you call me? I'm on duty here.")
        inf.add_link("A drunk.", dest = "drunkard2")

    elif msg == "drunkard2":
        inf.add_msg("I'm <b>NOT</b> a drunk!")
        inf.add_link("Yes you are.", dest = "drunkard3")

    elif msg == "drunkard3":
        inf.add_msg("NO! I'm on DUTY here! I haven't been *HIC* drinking!")
        inf.add_link("You're a drunkard.", dest = "drunkard4")

    elif msg == "drunkard4":
        inf.add_msg("You can't prove that I'm a drunkard!")
        inf.add_link("There's a beer barrel right next to you.", dest = "drunkard5")

    elif msg == "drunkard5":
        inf.add_msg("That's *HIC* it! Call me a drunkard again and you'll die!")
        inf.add_link("You're such a drunkard...", dest = "drunkard6")

    elif msg == "drunkard6":
        inf.dialog_close()
        me.Say("Now you've done it! Prepare to die!")
        me.enemy = activator

main()
inf.finish()
