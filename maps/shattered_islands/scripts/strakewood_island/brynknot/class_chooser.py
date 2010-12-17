## @file
## Implements the class chooser NPC.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## The possible classes.
classes = [
	{
		"name": "warrior",
		"msg": "As a warrior you've been trained in the art of combat with weapons and in archery.\nBecause of your training, you're stronger, more agile, and hardier than you would be otherwise.\nYour education, however, has not included studies in the magical arts or religious devotion, and, in general, lacks breadth.",
		"bonus": [
			"+20% hit points",
			"+2 Strength",
			"+1 Dexterity",
			"+1 Constitution",
		],
	},
	{
		"name": "sorcerer",
		"msg": "Your study of magic has been obsessive. Your frequent practice has greatly enhanced your powers, and your intellect has been sharpened enormously by your quest for ever better ways to channel energies.\nOn the other hand, you have totally neglected to learn to use weaponry, so you're soft and weak.",
		"bonus": [
			"+20% spell points",
			"+3 Power",
			"+2 Intelligence",
		],
	},
	{
		"name": "warlock",
		"msg": "You've divided your time between learning magic and learning weapons, but have totally disregarded religious devotion. You're physically stronger and hardier because of your training, and you know the use of weapons and bows. However, you're just a bit clumsy in both weaponry and magic because you've had to divide your time between them.",
		"bonus": [
			"+10% hit points",
			"+10% spell points",
			"+1 Strength",
			"+2 Power",
		],
	},
	{
		"name": "priest",
		"msg": "As a priest, you've learned an intense devotion to your god, and you've learned how to channel the energies your god vouchsafes to his devotees.\nYou've been taught the use of weapons, but only cursorily, and your physical training has been lacking in general.",
		"bonus": [
			"+20% grace points",
			"+3 Wisdom",
			"+2 Intelligence",
		],
	},
	{
		"name": "paladin",
		"msg": "You are a militant priest, with an emphasis on 'priest'.\nYou've been taught archery and the use of weapons, but great care has been taken that you're doctrinally correct. Now you've been sent out in the world to convert the unrighteous and destroy the enemies of the faith. Your church members have been charged a pretty penny to equip you for the job!",
		"bonus": [
			"+10% hit points",
			"+10% grace points",
			"+1 Strength",
			"+2 Wisdom",
		],
	},
]

def main():
	if msg == "hello" or msg == "hi" or msg == "hey" or msg == "back":
		me.SayTo(activator, "\nHello, {0}. I am the {1}.\nI will teach you the class of your choice. Now, tell me which class do you want more information about:\n{2}".format(activator.name, me.name, ", ".join(map(lambda d: "^" + d["name"] + "^", classes))))

	# Get a class.
	elif msg[:7] == "become ":
		# Do we have a class already?
		if activator.Controller().class_ob:
			return

		l = list(filter(lambda d: d["name"] == msg[7:], classes))

		if not l:
			return

		me.SayTo(activator, "\nAre you quite sure that you want to become {0}?\n\n^Yes, become {0}^".format(l[0]["name"]))

	# Confirmation for getting a class.
	elif msg[:12] == "yes, become ":
		# Do we have a class already?
		if activator.Controller().class_ob:
			return

		l = list(filter(lambda d: d["name"] == msg[12:], classes))

		if not l:
			return

		# Create the class object.
		activator.CreateObject("class_" + l[0]["name"])
		# fix_player() will be called eventually, so we just need to mark
		# the ext title for update.
		activator.Controller().s_ext_title_flag = True
		me.SayTo(activator, "\nCongratulations, you're {0} now!".format(l[0]["name"]))

	else:
		l = list(filter(lambda d: d["name"] == msg, classes))

		if not l:
			return

		# Tell the player about this class, and its bonuses.
		me.SayTo(activator, "\n{0}\n~Bonuses~:\n{1}".format(l[0]["msg"], "\n".join(l[0]["bonus"])))

		# Show a link to get the class or go back to the list of classes.
		if not activator.Controller().class_ob:
			me.SayTo(activator, "\n^Become {0}^ or go ^back^".format(l[0]["name"]), 1)

main()
