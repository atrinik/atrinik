from Atrinik import *
from Interface import Interface

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
inf = Interface(activator, me)

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		inf.add_msg("We did it! Now let us sail to Incuna, and hope that nothing terrible happens again...")
		inf.add_msg("You seem tired, however... But, I'm not surprised - you have done a lot of hard work, and I thank you for helping me repair the boat.")
		inf.add_msg("Now, go and have a good rest in the lower deck - I'll wake you up when we arrive.")
		inf.add_link("Very well.")

	elif msg == "very well.":
		inf.dialog_close()
		activator.TeleportTo("/shattered_islands/incuna/ship_lower_deck", 2, 2, sound = False)

main()
inf.finish()
