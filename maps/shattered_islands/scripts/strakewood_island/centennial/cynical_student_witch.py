## @file
## Implements the cynical student witch's responses.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Just cause she's a fan of Marisa doesn't mean that's all she's gotta go on about.  Geez.")
	elif msg == "reimu" or msg == "hakurei" or msg == "reimu hakurei":
		inf.add_msg("Now Reimu *is* totally awesome...")

main()
inf.finish()
