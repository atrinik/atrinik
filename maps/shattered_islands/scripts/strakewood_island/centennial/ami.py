## @file
## Script for Ami, the priestess.

from Interface import Interface
import Temple

inf = Interface(activator, me)

def main():
	lower_msg = msg.lower()
	if lower_msg == "sanae" or lower_msg == "kotiya" or lower_msg == "sanae kotiya" or lower_msg == "kochiya" or lower_msg == "sanae kochiya":
		inf.add_msg("Even though I'm an ocean priestess, I still think she's awesome.")
	elif lower_msg == "mizuno ami" or lower_msg == "ami mizuno" or lower_msg == "mizuno":
		inf.add_msg("Good guess, but how do you know my namesake wasn't Ami Kawashima, instead?")
		inf.add_msg("{} snickers.".format(me.name), COLOR_YELLOW)
	elif lower_msg == "kawashima ami" or lower_msg == "ami kawashima" or lower_msg == "kawashima":
		inf.add_msg("Maybe my namesake is really Ami Mizuno?  Who can say?  nihihihi...")
	else:
		temple = Temple.TempleDrolaxi(activator, me, inf)
		temple.handle_chat(msg)

main()
inf.finish()
