## @file
## This script implements the weapons master on Tutorial Island.
##
## The weapons master gives one of the four weapon skills, and a starting
## weapon.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Learn a weapon skill.
## @param skill Skill name to learn.
## @param weapon Weapon arch name to give to player.
def learn_weapon_skill(skill, weapon):
	## Get the skill number.
	skill_nr = GetSkillNr(skill)

	if skill_nr == -1:
		me.SayTo(activator, "\nUnknown skill.")
	else:
		if activator.DoKnowSkill(skill_nr) == 1:
			me.SayTo(activator, "\nYou already know this skill.")
		else:
			activator.AcquireSkill(skill_nr)
			obj = activator.CreateObject(weapon, value = 1)
			activator.Write("{0} gives you a {1}.".format(me.name, obj.name), 0)
			activator.Apply(obj, 0)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello! I am {0}.\nI will give you your starting weapon skill and your first weapon. Now, tell me which weapon skill do you want.\nYou can select between slash weapons, cleave weapons, pierce weapons or impact weapons.\nAsk me about ^weapons^ to learn more.".format(me.name))

	elif msg == "slash":
		learn_weapon_skill("slash weapons", "shortsword")

	elif msg == "impact":
		learn_weapon_skill("impact weapons", "mstar_small")

	elif msg == "cleave":
		learn_weapon_skill("cleave weapons", "axe_small")

	elif msg == "pierce":
		learn_weapon_skill("pierce weapons", "dagger_large")

	elif msg == "weapons":
		me.SayTo(activator, "\nWe have 4 different weapon skills.\nEach skill allows the use of a special kind of weapons.\nSlash weapons are swords.\nCleave weapons are axe-like weapons.\nPierce weapons are rapiers and daggers.\nImpact weapons are maces or hammers.\nNow select one and tell me: ^slash^, ^cleave^, ^pierce^ or ^impact^?")

if activator.DoKnowSkill(GetSkillNr("impact weapons")) == 1 or activator.DoKnowSkill(GetSkillNr("slash weapons")) == 1 or activator.DoKnowSkill(GetSkillNr("cleave weapons")) == 1 or activator.DoKnowSkill(GetSkillNr("pierce weapons")) == 1:
	me.SayTo(activator, "\nYou already know a weapon skill.")
else:
	main()
