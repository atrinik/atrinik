## @file
## Generic script for guards.

from Interface import Interface

inf = Interface(activator, me)

def main():
	if msg == "hello":
		import random

		replies = [
			"Stay out of trouble and you won't get hurt.",
			"Move along.",
			"Watch yourself. We don't want any trouble here.",
			"Watch yourself, stranger.",
			"I have my eye on you.",
			"I wouldn't cause any fuss if I were you.",
			"We're watching you.",
		]

		inf.add_msg(random.choice(replies))

main()
inf.finish()
