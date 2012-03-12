## @file
## Script for Silmedsen's potion bottle.

def main():
	SetReturnValue(1)

	for (m, x, y) in activator.SquaresAround(1):
		for obj in m.GetLayer(x, y, LAYER_FLOOR):
			if obj.type == Type.FLOOR and obj.name == "Morliana water":
				me.Remove()
				activator.CreateObject("silmedsen_potion_bottle_filled")
				pl.DrawInfo("You fill the bottle to the brim with the clear water.", COLOR_GREEN)
				return

	pl.DrawInfo("You can't fill the bottle with that... You must face the water around the Great Blue Crystal in Morliana.", COLOR_ORANGE)

main()
