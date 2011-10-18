## @file
## Implements Kanako, the instructor's dialog.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Hey.  You're not in my summoning class...")
	elif msg == "sanae" or msg == "kotiya" or msg == "sanae kotiya" or msg == "kochiya" or msg == "sanae kochiya":
		inf.add_msg("I'll remind you not to pry into other people's hobbies, thank you.")
	elif msg == "kanako yasaka" or msg == "yasaka":
		inf.add_msg("Somebody wants to be put through the wringer, do they?")

main()
inf.finish()
