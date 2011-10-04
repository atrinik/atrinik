## @file
## Script for spell sellers.

from Interface import Interface

inf = Interface(activator, me)

def main():
	spells = GetOptions().split(",")

	if msg == "hello":
		inf.add_msg("Welcome, dear customer! I'm {}.".format(me.name))
		inf.add_msg("Are you here to buy one of my {0}? I can offer you the following {0}.".format({"wizard": "spells", "priest": "prayers"}[GetSpell(GetSpellNr(spells[0]))["type"]]))

		for spell in spells:
			inf.add_link(spell.capitalize(), dest = "buy1 " + spell)

	elif msg.startswith("buy1 "):
		name = msg[5:]

		if not name in spells:
			return

		spell_nr = GetSpellNr(name)
		spell = GetSpell(spell_nr)

		inf.add_msg("Ah, so you want to buy {}?".format(name))
		inf.add_msg_icon(spell["icon"], spell["desc"], fit = True)

		if activator.Controller().DoKnowSpell(spell_nr):
			inf.add_msg("... but it seems you already know {}.".format(name))
			return

		skill_name = {"wizard": "wizardry spells", "priest": "divine prayers"}[spell["type"]]
		skill = activator.Controller().GetSkill(Type.SKILL, GetSkillNr(skill_name))
		color = COLOR_GREEN if skill.level >= spell["level"] else COLOR_RED
		inf.add_msg("{} requires level to {} use. Your {} skill is level {}.".format(name.capitalize(), spell["level"], skill_name, skill.level), color)

		inf.add_msg("{} will cost you {}. Is that okay?".format(name.capitalize(), CostString(spell["cost"])))
		inf.add_link("Buy {}".format(name), dest = "buy2 " + name)

	elif msg.startswith("buy2 "):
		name = msg[5:]

		if not name in spells:
			return

		spell_nr = GetSpellNr(name)

		if activator.Controller().DoKnowSpell(spell_nr):
			return

		spell = GetSpell(spell_nr)

		if activator.PayAmount(spell["cost"]):
			inf.add_msg("You pay {}.".format(CostString(spell["cost"])), COLOR_YELLOW)
			inf.add_msg("Pleasure doing business with you!")
			inf.add_msg_icon(spell["icon"], name, fit = True)
			activator.Controller().AcquireSpell(spell_nr)
		else:
			inf.add_msg("You don't have enough money...")

main()
inf.finish()
