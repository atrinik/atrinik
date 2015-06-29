## @file
## Script for the main entrance of the Abandoned Mine; gate that requires
## a passphrase.

from Interface import Interface

inf = Interface(activator, me)

def main():
    lever = me.map.LocateBeacon("abandoned_mine_gate_lever").env

    # The lever has been activated so the gate is (going) down.
    if lever.speed:
        SetReturnValue(1)
        return

    inf.set_icon("gate_door1.111")
    inf.set_title("Gate")

    if msg and msg.lower() == "mellon":
        inf.dialog_close()
        lever.Apply(lever)
    else:
        inf.add_msg("You notice a strange writing on the gate...", COLOR_YELLOW)
        inf.add_msg("[font=pecita][size=+8][o=#0083ff][center]Pedo mellon a mino![/center][/o][/size][/font]")
        inf.add_msg("It seems some kind of passphrase is needed to activate it.", COLOR_YELLOW)
        inf.set_text_input()

main()
inf.send()
