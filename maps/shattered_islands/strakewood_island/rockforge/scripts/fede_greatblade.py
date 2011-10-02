## @file
## Script for Fede Greatblade in Rockforge.

from Interface import Interface

inf = Interface(activator, me)
msg = msg.lower()

def main():
	location = GetOptions()

	if location == "dining_hall":
		if msg == "hello":
			inf.add_msg("No, you can't fool me! That cake is surely a lie! ... or is it? Hmm....")

		elif msg == "portal":
			inf.add_msg("What? I just want cake!")

	elif location == "house":
		if msg == "hello":
			inf.add_msg("The dwarf appears to be sleeping like a stone... Suddenly, he yells out, as if from a dream:", COLOR_YELLOW)
			inf.add_msg("This is vewrry!!! Weird!!!")

		elif msg == "shutdown":
			inf.add_msg("Aaahhhhhhhhhhhhhhhhhhhhhhh!!!!!")

		elif msg == "faction":
			inf.add_msg("*snort*")
			inf.add_msg("The dwarf continues sleeping...", COLOR_YELLOW)

main()
inf.finish()
