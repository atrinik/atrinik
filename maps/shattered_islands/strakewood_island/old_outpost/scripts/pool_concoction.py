## @file
## Script for concoction pools in Old Outpost.

from QuestManager import QuestManager
from InterfaceQuests import gandyld_mana_crystal

options = GetOptions()
qm = QuestManager(activator, gandyld_mana_crystal)

def main():
    if not qm.need_finish(options):
        return

    SetReturnValue(1)

    obj = activator.map.CreateObject("light9", activator.x, activator.y)
    obj.speed = 0.5
    obj.f_is_used_up = True
    obj.food = 1

    old = activator.FindObject(name = gandyld_mana_crystal["parts"][options]["item"]["name"])
    new = activator.CreateObject(gandyld_mana_crystal["parts"][options]["item"]["arch"])

    # If the old mana crystal exists, preserve number of spell points the
    # player may have stored inside it, then destroy it.
    if old:
        new.sp = old.sp
        old.Destroy()

    if options == "enhance_crystal_alchemists":
        pl.DrawInfo("\nYou dip the mana crystal into the pool. A flash of light occurs, and the crystal seems to be glowing even brighter than before... Perhaps you should report to Gandyld.", color = COLOR_YELLOW)
        activator.Controller().Sound("learnspell.ogg")
    elif options == "enhance_crystal_rhun":
        pl.DrawInfo("\nYou dip the mana crystal into the pool. A flash of bright light occurs, followed by a loud crackling noise, and the crystal seems to be glowing even brighter than before... Perhaps you should report to Gandyld.", color = COLOR_YELLOW)
        activator.Controller().Sound("magic_elec.ogg")

main()
