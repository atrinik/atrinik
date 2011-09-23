## @file
## The weapons master inside the Tutorial Cave.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Learn a weapon skill.
## @param skill Skill name to learn.
## @param weapon Weapon arch name to give to player.
def learn_weapon_skill(skill, weapon):
	# Get the skill number.
	skill_nr = GetSkillNr(skill)

	if skill_nr == -1:
		me.SayTo(activator, "\nFatal error: unknown skill.")
		return

	if activator.Controller().DoKnowSkill(skill_nr):
		me.SayTo(activator, "\nYou already know this skill.")
		return

	activator.Controller().AcquireSkill(skill_nr)
	obj = activator.CreateObject(weapon, value = 1)
	me.SayTo(activator, "\nA fine choice! But you will need a weapon too...\n<yellow>{} gives you a {}.</yellow>\nNow, please proceed through the door to learn about combat.".format(me.name, obj.name))
	activator.Apply(obj, 0)

	force = activator.CreateForce("tutorial_weapons_master")
	force.slaying = "tutorial_weapons_master"

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello! I am {}.\nI will give you your starting weapon skill and your first weapon. Now, tell me which weapon skill do you want.\nYou can select between slash weapons, cleave weapons, pierce weapons or impact weapons.\nAsk me about <a>weapons</a> to learn more.".format(me.name))

	elif msg == "slash":
		learn_weapon_skill("slash weapons", "shortsword")

	elif msg == "impact":
		learn_weapon_skill("impact weapons", "mstar_small")

	elif msg == "cleave":
		learn_weapon_skill("cleave weapons", "axe_small")

	elif msg == "pierce":
		learn_weapon_skill("pierce weapons", "dagger_large")

	elif msg == "weapons":
		me.SayTo(activator, "\nWe have 4 different weapon skills.\nEach skill allows the use of a special kind of weapons.\nSlash weapons are swords.\nCleave weapons are axe-like weapons.\nPierce weapons are rapiers and daggers.\nImpact weapons are maces or hammers.\nNow select one and tell me: <a>slash</a>, <a>cleave</a>, <a>pierce</a> or <a>impact</a>?")

if activator.Controller().DoKnowSkill(GetSkillNr("impact weapons")) or activator.Controller().DoKnowSkill(GetSkillNr("slash weapons")) or activator.Controller().DoKnowSkill(GetSkillNr("cleave weapons")) or activator.Controller().DoKnowSkill(GetSkillNr("pierce weapons")):
	me.SayTo(activator, "\nYou already know a weapon skill. Please proceed through the door to learn about combat.")
else:
	main()
