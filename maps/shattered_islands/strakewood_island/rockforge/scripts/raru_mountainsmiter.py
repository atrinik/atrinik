## @file
## Script for Raru Mountainsmiter in Rockforge.

from Interface import Interface

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Hello there! My name is Raru Mountainsmiter, but please excuse me, I'm about to start eating.")
		inf.add_link("Are you?", dest = "eating")
		inf.add_link("Sorry.", action = "close")

	elif msg == "eating":
		inf.add_msg("Well, since this is the Dining Hall, yes! Well, as soon as the cooks quit being lazy and bring me my meal...")
		inf.add_msg("{} yells out towards cooks:".format(me.name), COLOR_YELLOW)
		inf.add_msg("Oi! Hurry up, you lazy cooks!")

main()
inf.finish()
