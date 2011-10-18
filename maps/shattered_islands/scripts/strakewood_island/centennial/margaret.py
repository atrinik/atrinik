## @file
## Implements Margaret, the headmistress's dialog.

from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

def main():
	if msg == "hello":
		inf.add_msg("Welcome to the Centennial Witch Academy.  How can I help you, {}?".format(activator.name))
		inf.add_link("Ask for some help.", dest = "help")
	elif msg == "help":
		inf.add_msg("As the headmistress of the school, it is my duty to ensure all of our guests are taken care of.  Let me know if you need something specific.")
	elif msg == "alice" or msg == "margatroid" or msg == "alice margatroid":
		inf.add_msg("{} laughs.".format(me.name), COLOR_YELLOW)
		inf.add_msg("That's a pretty amusing association you have made.")

main()
inf.finish()
