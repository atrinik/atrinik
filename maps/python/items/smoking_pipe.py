## @file
## Script for the smoking pipe item.

from Atrinik import *

activator = WhoIsActivator()

def main():
	# Safety.
	if activator.type != TYPE_PLAYER:
		return

	# Find the marked pipeweed.
	marked = activator.FindMarkedObject()

	if not marked:
		activator.Write("You need to mark the object you want to smoke.", COLOR_BLUE)
		return

	# Not pipeweed?
	if marked.arch.name != "pipeweed":
		activator.Write("You can't smoke that.", COLOR_BLUE)
		return

	# Have we smoked lately?
	if activator.CheckInventory(0, "force", "pipeweed_force"):
		activator.Write("That was a nice smoke, but you'll have to wait for its effects to lessen before taking another...", COLOR_BLUE)
		return

	force = activator.CreateObjectInside("force", 1, 1)
	force.name = "pipeweed_force"
	force.type = TYPE_POTION_EFFECT
	force.f_is_used_up = True
	force.speed = 0.1
	force.food = 50
	force.Con = -3
	force.Dex = -3
	force.f_applied = True

	# Cursed or damned pipeweed? Worsen the stat effects...
	if marked.f_cursed or marked.f_damned:
		activator.Write("Ack, that was some rotten pipeweed!", COLOR_RED)
		force.Int = -5
		force.Wis = -5
	else:
		force.SetProtection(ATNR_CONFUSION, 25)
		force.SetProtection(ATNR_WEAPON_MAGIC, 20)

	activator.ChangeAbil(force)
	# Decrease number of pipeweeds.
	marked.Decrease()

try:
	main()
finally:
	SetReturnValue(1)
