from Atrinik import *
from Interface import Interface

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().lower().strip()

inf = Interface(activator, me)

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		inf.add_msg("Well, hello there! Would you care for a refreshing bottle of booze?")
		inf.add_link("Sure, thanks.")
		inf.add_link("No thanks.")
		inf.add_link("<a=close:>&lt;exit&gt;</a>")

	elif msg == "sure, thanks.":
		inf.add_msg("Fooled ya!")

	elif msg == "no thanks.":
		inf.add_msg("Oh man!")

main()
inf.finish()
