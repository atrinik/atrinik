## @file
## This script implements the weapons master in Tutorial Cave.
##
## The weapons master gives one of the four weapon skills, and a starting
## weapon.
##
## Inserts a marker to proceed to the next room after receiving a skill.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

def give_weapon(skill, weapon_arch):
	skill = GetSkillNr(skill)

	if skill == -1:
		me.SayTo(activator, "Unknown skill.")
	else:
<<<<<<< TREE
		if activator.DoKnowSkill(skill):
			me.SayTo(activator, "You already know this skill.")
=======
		if activator.Controller().DoKnowSkill(skill_nr):
			me.SayTo(activator, "\nYou already know this skill.")
>>>>>>> MERGE-SOURCE
		else:
<<<<<<< TREE
<<<<<<< TREE
			activator.AcquireSkill(skill)
			weapon = activator.CreateObjectInside(weapon_arch, 1, 1, 1)
			activator.Apply(weapon, 0)
			activator.Write("%s gives you a %s." % (me.name, weapon.name), 0)
			force = activator.CreatePlayerForce("tutorial_weapons_master")
			force.slaying = "tutorial_weapons_master"

if activator.DoKnowSkill(GetSkillNr("impact weapons")) or activator.DoKnowSkill(GetSkillNr("slash weapons")) or activator.DoKnowSkill(GetSkillNr("cleave weapons")) or activator.DoKnowSkill(GetSkillNr("pierce weapons")):
=======
			activator.AcquireSkill(skill_nr)
=======
			activator.Controller().AcquireSkill(skill_nr)
>>>>>>> MERGE-SOURCE
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

<<<<<<< TREE
if activator.DoKnowSkill(GetSkillNr("impact weapons")) == 1 or activator.DoKnowSkill(GetSkillNr("slash weapons")) == 1 or activator.DoKnowSkill(GetSkillNr("cleave weapons")) == 1 or activator.DoKnowSkill(GetSkillNr("pierce weapons")) == 1:
>>>>>>> MERGE-SOURCE
=======
if activator.Controller().DoKnowSkill(GetSkillNr("impact weapons")) or activator.Controller().DoKnowSkill(GetSkillNr("slash weapons")) or activator.Controller().DoKnowSkill(GetSkillNr("cleave weapons")) or activator.Controller().DoKnowSkill(GetSkillNr("pierce weapons")):
>>>>>>> MERGE-SOURCE
	me.SayTo(activator, "\nYou already know a weapon skill.")

elif msg == "slash":
	give_weapon("slash weapons", "shortsword")

elif msg == "impact":
	give_weapon("impact weapons", "mstar_small")

elif msg == "cleave":
	give_weapon("cleave weapons", "axe_small")

elif msg == "pierce":
	give_weapon("pierce weapons", "dagger_large")

elif msg == "weapons":
	me.SayTo(activator, "\nWe have 4 different weapon skills.\nEach skill allows the use of a special kind of weapons.\nSlash weapons are swords.\nCleave weapons are axe-like weapons.\nPierce weapons are rapiers and daggers.\nImpact weapons are maces or hammers.\nNow select one and tell me: ^slash^, ^cleave^, ^pierce^ or ^impact^?")

elif msg == "hello" or msg == "hi" or msg == "hey":
	me.SayTo(activator, "\nHello! I am %s.\nI will give you your starting weapon skill and your first weapon. Now, tell me which weapon skill do you want.\nYou can select between slash weapons, cleave weapons, pierce weapons or impact weapons.\nAsk me about ^weapons^ to learn more." % me.name)

else:
	activator.Write("%s seems not to notice you.\nYou should try ^hello^, ^hi^ or ^hey^..." % me.name, 0)