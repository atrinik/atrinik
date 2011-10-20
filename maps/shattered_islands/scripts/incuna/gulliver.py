from Interface import Interface
from Packet import Notification

inf = Interface(activator, me)

def main():
	pl = activator.Controller()

	if not "incuna" in pl.region_maps:
		if msg == "hello":
			inf.add_msg("Hello there! I'm {}, the dockmaster. You must be new around here, for I do not recognize you. Are you the adventurer Sam Goodberry spoke of?".format(me.name))
			inf.add_link("Yes...", dest = "yes")

		elif msg == "yes":
			inf.add_msg("Hmm! Sam is a good friend of mine, and a great captain. Shame about what happened to you - from what I gather, you hit your head while on the sea in that storm?")
			inf.add_link("Er... Yes...", dest = "er... yes...")
			inf.add_link("&lt;no comment&gt;", dest = "nocomment")

		elif msg == "er... yes...":
			inf.add_msg("Must have been painful, that! How's your memory doing?")
			inf.add_link("Not so good...", dest = "nocomment")
			inf.add_link("&lt;no comment&gt;", dest = "nocomment")

		elif msg == "nocomment":
			inf.add_msg("Hmm... well... at any rate, what is the reason behind your visit?")
			inf.add_link("Sam mentioned you might have a spare map of Incuna?", dest = "map")
			inf.add_link("Just looking around...", dest = "lookingaround")

		elif msg == "lookingaround":
			inf.add_msg("I see...")
			inf.add_link("Sam mentioned you might have a spare map of Incuna?", dest = "map")

		elif msg == "map":
			inf.add_msg("I do indeed - I have several spare maps here in fact. Since you're new around here, it is only proper I should give you one. Here you go...")
			inf.add_msg("{} gives you a map of Incuna.".format(me.name), COLOR_YELLOW)
			pl.region_maps.append("incuna")
			inf.add_link("Thanks.", action = "close")
			Notification(pl, "Tutorial Available: Region Map", "/help basics_region_map", "?HELP", 90000)

	else:
		if msg == "hello":
			inf.add_msg("Ah, you're back! I'd love to chat a bit more, but I've got paperwork to fix... Perhaps some other time?")

main()
inf.finish()
