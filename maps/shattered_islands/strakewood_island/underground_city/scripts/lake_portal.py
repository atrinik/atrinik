## @file
## Script for the Brynknot Lake portal in UC I.

import random
from QuestManager import QuestManagerMulti
from Quests import LlwyfenPortal as quest

qm = QuestManagerMulti(activator, quest)

def main():
    if not qm.completed("portal found"):
        pl.DrawInfo("The portal bounces you away as soon as you touch it. It appears to be sealed by the powers of the elven god Llwyfen.", COLOR_RED)

        if not qm.started("portal found"):
            pl.DrawInfo("Perhaps you should search for a priest of Llwyfen to learn more.", COLOR_YELLOW)
            qm.start("portal found")

        d = random.randint(1, SIZEOFFREE1)
        activator.TeleportTo(activator.map.path, activator.x + freearr_x[d], activator.y + freearr_y[d])
        SetReturnValue(1)
    else:
        amulet = activator.FindObject(INVENTORY_CONTAINERS, "amulet_llwyfen")

        if amulet:
            pl.DrawInfo("Upon coming into contact with the portal, the amulet of Llwyfen shatters and you feel as if an invisible force was being removed! The seal is broken.", COLOR_GREEN)
            amulet.Remove()

main()
