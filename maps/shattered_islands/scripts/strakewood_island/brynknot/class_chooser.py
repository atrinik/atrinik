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
			"+2 AC",
			"+2 Strength",
			"+1 Dexterity",
			"+1 Constitution",
		],
	},
	{
		"name": "archer",
		"msg": "As an archer, you have learned the art of using long range weapons such as bows and crossbows.\nYou have been taught how to read the wind in order to strike down enemies more effectively, but you have neglected religious devotion.\nYou have also trained your strength in order to be able to fire missiles from ranged weapons with enough strength to cause the most damage to your enemies.",
		"bonus": [
			"+5% hit points",
			"+2 Strength",
			"+3 Dexterity",
			"+1 Intelligence",
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
		me.SayTo(activator, "\nHello, {}. I am the {}.\nI will teach you the class of your choice. Now, tell me which class do you want more information about:\n{}".format(activator.name, me.name, ", ".join(map(lambda d: "<a>" + d["name"] + "</a>", classes))))

	# Get a class.
	elif msg[:7] == "become ":
		# Do we have a class already?
		if activator.Controller().class_ob:
			return

		l = list(filter(lambda d: d["name"] == msg[7:], classes))

		if not l:
			return

		me.SayTo(activator, "\nAre you quite sure that you want to become {0}?\n\n<a>Yes, become {0}</a>".format(l[0]["name"]))

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
		me.SayTo(activator, "\nCongratulations, you're {} now!".format(l[0]["name"]))

	else:
		l = list(filter(lambda d: d["name"] == msg, classes))

		if not l:
			return

		# Tell the player about this class, and its bonuses.
		me.SayTo(activator, "\n{}\n<green>Bonuses</green>:\n{}".format(l[0]["msg"], "\n".join(l[0]["bonus"])))

		# Show a link to get the class or go back to the list of classes.
		if not activator.Controller().class_ob:
			me.SayTo(activator, "\n<a>Become {}</a> or go <a>back</a>".format(l[0]["name"]), True)

main()
