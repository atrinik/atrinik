## @file
## Script for Sakura the priestess.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
	lower_msg = msg.lower()
	if lower_msg == "reimu" or lower_msg == "hakurei" or lower_msg == "reimu hakurei":
		inf.add_msg("{} blushes.".format(me.name), COLOR_YELLOW)
		inf.add_msg("Yeah, I kinda feel like she's kindred spirit or something.")
		inf.add_msg("{} glances over at donations.".format(me.name), COLOR_YELLOW)
	elif lower_msg == "kinomoto" or lower_msg == "sakura kinomoto":
		inf.add_msg("Oh, fine... I'll say it.")
		inf.add_msg("{} grabs a baton with a star on it and does a dance.".format(me.name), COLOR_YELLOW)
		inf.add_msg("\"Keiyaku no moto Sakura ga mejiru.  Release!\"", COLOR_PINK)
		inf.add_msg("There.  Happy?")
	temple = Temple.TempleTerria(activator, me, inf)
	temple.handle_chat(msg)

main()
inf.finish()
