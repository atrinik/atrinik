## @file
## Implements Usagi, the instructor's dialog.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Hey!  You don't look like one of my students...")
	elif msg == "marisa" or msg == "kirisame" or msg == "marisa kirisame":
		inf.add_msg("Hey, I don't knock your hobbies.")
	elif msg == "sailor moon" or msg == "tsukino" or msg == "usagi tsukino":
		inf.add_msg("No, I won't do an \"In the name of the Moon\" speech for you.  Go away.")

main()
inf.finish()