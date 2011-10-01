## @file
## Implements the smug student witch's card game responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Heh.  This sucker keeps falling for the same trick.")
		inf.add_link("Are you cheating?")
	elif msg == "are you cheating?":
		inf.add_msg("It ain't cheating, I'm just playing creatively.  Haha.~")
	elif msg == "marisa" or msg == "kirisame" or msg == "marisa kirisame":
		inf.add_msg("Marisa's awesome isn't she?")

main()
inf.finish()
