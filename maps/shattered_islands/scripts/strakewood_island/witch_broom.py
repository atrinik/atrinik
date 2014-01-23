## @file
## Dialog for the witch in swamp near Asteria.

from Interface import Interface

inf = Interface(activator, me)

def main():
    # Don't say anything if we have an enemy; she's focusing on the enemy.
    if me.enemy:
        inf.dialog_close()
        return

    if msg == "hello":
        inf.add_msg("Welcome, welcome... Heh-heh-heh...")
        inf.add_link("Why do you have two brooms?", dest = "havetwo")

    elif msg == "havetwo":
        inf.add_msg("That is none of your concern! Now go away before I get really mad!")
        inf.add_link("Did you steal one?", dest = "stealone")

    elif msg == "stealone":
        inf.add_msg("Keep it up, and you'll get more than you bargained for!")
        inf.add_link("You stole it, didn't you.", dest = "stoleit")

    elif msg == "stoleit":
        me.Say("Now you've done it!")
        inf.dialog_close()

        # The witch gets angry...
        me.enemy = activator
        # Find witch's cat spawn point.
        obj = me.map.GetLastObject(14, 12)

        # Check if it really is a spawn point and whether there is a spawned
        # monster, if so, set the spawned monster's enemy to activator too.
        if obj.type == Type.SPAWN_POINT and obj.enemy:
            obj.enemy.enemy = activator

main()
inf.finish()
