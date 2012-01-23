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
			"Keep moving.",
			"Watch yourself. We don't want any trouble here.",
			"Watch yourself, stranger.",
			"I have my eye on you.",
			"I would not cause any fuss if I were you.",
			"We're watching you.",
			"You seem to be new around here. Make sure you do not cross the law.",
			"Start any trouble and you'll have to deal with me.",
			"I'm the law around here and don't you forget it.",
			"Cross me and you might get more than you bargained for.",
			"Welcome. We do not tolerate law breakers here.",
			"I'm not going to cross you, unless you give me a reason to.",
			"It would not be wise at all for you to anger me.",
			"You're welcome here, as long as you keep to the law.",
			"Not now, my shift is almost over.",
			"You should respect the law, for I am the law here.",
			"We welcome strangers, but law breakers will not be taken kindly.",
			"I hope you're not here to cause any sort of trouble.",
			"If you're here to cause trouble, things might get particularly unpleasant for you.",
			"What is it, stranger? We do not want any trouble here, so move along.",
			"There is something suspicious about you. I'll be watching you.",
			"You better not be up to anything unlawful.",
			"Have you come here to cause trouble? If so, you came to the wrong place.",
			"I haven't seen you before, so I'll make a deal with you. Stay out of trouble and I won't have a reason to hurt you.",
			"You look like a shady character. Make one wrong move and you will wish you had never came here.",
			"You have my ear, citizen.",
			"Speak.",
			"What is it, citizen?",
			"Is there a problem?",
			"Yes, citizen? What is it?",
			"Disrespect the law, and you disrespect me.",
		]

		inf.add_msg(random.choice(replies))

main()
inf.finish()
