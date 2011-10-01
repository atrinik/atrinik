## @file
## Implements the tomboyish fairy.

import random
from Interface import Interface

msg = msg.lower()

inf = Interface(activator, me)

_idiot_cirnoisms = ["Hey!  Eye'm not dumb.", "The one saying \"idiot\" is an idiot."]
_angry_cirnoisms = ["Eye said Eye'm the strongest!", "I'll cryo-freeze you along with some English beef!", "This atmosphere... it smells like WAR!", "I'm going to beat you for that.", "Freeze to death! Freeze to death!"]
_happy_cirnoisms = ["Yay!", "Yeah. I'm the best.", "There are no buses in Gensokyo.", "Eye'm a genius!", "So, what was I doing, again...?"]

def main():
	if msg == "hello":
		inf.add_msg("Eye'm the strongest!")
		inf.add_link("You're the strongest?", dest = "the strongest")
		inf.add_link("No way, that's impossible!", dest = "not the strongest")
		inf.add_link("You're the strongest fairy... maybe.", dest = "strongest fairy")
		inf.add_link("What?  Are you dumb or something?", dest = "idiot")
		inf.add_link("Goodbye.", action = "close")
	elif msg == "idiot" or msg == "baka" or msg == "nineball" or msg == "marukyuu" or msg == "(9)":		
		inf.add_msg(random.choice(_idiot_cirnoisms))
		inf.add_link("OK.", dest = "hello")
		inf.add_link("That was childish.", dest = "idiot")
		inf.add_link("Goodbye.", action = "close")
	elif msg == "not the strongest" or msg == "weak" or msg == "strongest fairy":
		inf.add_msg(random.choice(_angry_cirnoisms))
		inf.add_link("Whatever.", dest = "hello")
		inf.add_link("You must be stupid or something.", dest = "idiot")
		inf.add_link("Oh, you must think you're Cirno.", dest = "cirno")
		inf.add_link("Goodbye.", action = "close")
	elif msg == "the strongest" or msg == "strong" or msg == "cirno":
		inf.add_msg(random.choice(_happy_cirnoisms))
		inf.add_link("OK.", dest = "hello")
		inf.add_link("Well, that was stupid.", dest = "idiot")
		inf.add_link("Goodbye.", action = "close")

main()
inf.finish()
