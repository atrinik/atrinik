## @file
## Script for Hoofyard, NPC which teaches players the construction skill.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Cost of the construction skill.
cost = 100000
## Minimum required level.
req_level = 45

def main():
	skill_nr = GetSkillNr("construction")
	has_skill = activator.GetSkill(TYPE_SKILL, skill_nr)
	print(has_skill)

	if msg == "hi" or msg == "hey" or msg == "hello":
		if has_skill:
			me.SayTo(activator, "\nYou can buy materials in the shop around here in Everlink to use with your construction skill.\nDo you need ^information^ about the construction skill?")
		else:
			me.SayTo(activator, "\nAh, a visitor! Do you have the ^construction^ skill yet?")

	elif msg == "information":
		me.SayTo(activator, "\nTo use the construction skill, you need to buy a construction builder and a construction destroyer. With the destroyer, you can destroy previously built objects or walls. You will need a destroyer to destroy the extra walls in your house (in Greyton house, this is the eastern wall, for example). Use the builder to build walls, windows, fireplaces, and so on. To use either the destroyer or the builder, you must first apply it, and then type ~/use_skill construction~ or find the construction skill in your skill list, press enter and use CTRL + direction.\n\n^Information about materials^")

	elif msg == "information about materials":
		me.SayTo(activator, "\nTo build using the construction builder, you need to mark a material you want to build. For example, an altar material, desk material, or wall material. Sign materials exist too, but to build them, you need a book on the square where you want to build the sign with the message you want the sign to have (custom name of the book will be copied to the sign). Windows, pictures, flags and so on can only be built on top of walls.")

	elif msg == "construction" and not has_skill:
		me.SayTo(activator, "\nThe construction skill allows you to build inside your house, you see. Building requires materials that you can buy in the shops around, but you need the construction skill first.\nWould you want me to teach you the construction skill, for a small fee of {0}?\n\n^Teach me^".format(CostString(cost)))

	elif msg == "teach me" and not has_skill:
		if activator.level < req_level:
			me.SayTo(activator, "\nYou are too low level. Come back when you are at least level {0}.".format(req_level))
		elif activator.PayAmount(cost) == 1:
			me.SayTo(activator, "\nVery well then.\n~{0} teaches you the construction skill!~\nThank you, I hope it will serve you well.\nDo you need ^information^ about the construction skill now?".format(me.name))
			activator.AcquireSkill(skill_nr)
		else:
			me.SayTo(activator, "\nYou do not have enough money.")

main()
