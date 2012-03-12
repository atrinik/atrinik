## @file
## Script for concoction pools in Old Outpost.

from QuestManager import QuestManagerMulti
from Quests import GandyldsManaCrystal as quest

location = GetOptions()
qm = QuestManagerMulti(activator, quest)

def crystal_exchange(arch1, arch2):
	old = activator.FindObject(archname = arch1)
	new = activator.CreateObject(arch2)
	# Preserve number of spell points.
	new.sp = old.sp
	# Remove old one.
	old.Remove()

# Flash of light effect.
def flash_light():
	obj = activator.map.CreateObject("light9", activator.x, activator.y)
	obj.speed = 0.5
	obj.f_is_used_up = True
	obj.food = 1

def main():
	if location == "lab":
		if qm.started_part(1) and not qm.finished(1) and not qm.completed_part(1):
			crystal_exchange("gandyld_crystal_1", "gandyld_crystal_2")
			pl.DrawInfo("\nYou dip the mana crystal into the pool. A flash of light occurs, and the crystal seems to be glowing even brighter than before... Perhaps you should report to Gandyld.", COLOR_YELLOW)
			flash_light()
			activator.Controller().Sound("learnspell.ogg")
			SetReturnValue(1)

	elif location == "lab_rhun":
		if qm.started_part(2) and not qm.finished(2) and not qm.completed_part(2):
			crystal_exchange("gandyld_crystal_2", "gandyld_crystal_3")
			pl.DrawInfo("\nYou dip the mana crystal into the pool. A flash of bright light occurs, followed by a loud crackling noise, and the crystal seems to be glowing even brighter than before... Perhaps you should report to Gandyld.", COLOR_YELLOW)
			flash_light()
			activator.Controller().Sound("magic_elec.ogg")
			SetReturnValue(1)

main()
