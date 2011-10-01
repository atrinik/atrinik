## @file
## Script for Kendo Flamecutter in Rockforge.

from Interface import Interface

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Lembas... again....")
		inf.add_link("What's wrong?", dest = "whatwrong")

	elif msg == "whatwrong":
		inf.add_msg("The cooks just keep purchasing waybread from the elves...")
		inf.add_link("How come?", dest = "howcome")

	elif msg == "howcome":
		inf.add_msg("{} grumbles.".format(me.name), COLOR_YELLOW)
		inf.add_msg("Lazy buggers, that's my guess...")

main()
inf.finish()
