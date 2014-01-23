## @file
## Script for spell sellers.

from Interface import Interface

inf = Interface(activator, me)

def main():
    spells = GetOptions().split(",")

    if msg == "hello":
        inf.add_msg("Welcome, dear customer! I'm {}.".format(me.name))
        inf.add_msg("Are you here to buy one of my spells? I can offer you the following spells.")

        for spell in spells:
            inf.add_link(spell.capitalize(), dest = "buy1 " + spell)

    elif msg.startswith("buy1 "):
        name = msg[5:]

        if not name in spells:
            return

        spell = GetArchetype("spell_" + name.replace(" ", "_")).clone

        inf.add_msg("Ah, so you want to buy {}?".format(name))
        inf.add_msg_icon(spell.face[0], spell.msg, fit = True)

        if activator.FindObject(type = Type.SPELL, name = name):
            inf.add_msg("... but it seems you already know {}.".format(name))
            return

        skill = activator.FindObject(type = Type.SKILL, name = "wizardry spells")
        color = COLOR_GREEN if skill.level >= spell.level else COLOR_RED
        inf.add_msg("{} requires level {} to use. Your wizardry spells skill is level {}.".format(name.capitalize(), spell.level, skill.level), color)

        inf.add_msg("{} will cost you {}. Is that okay?".format(name.capitalize(), CostString(spell.value)))
        inf.add_link("Buy {}".format(name), dest = "buy2 " + name)

    elif msg.startswith("buy2 "):
        name = msg[5:]

        if not name in spells:
            return

        if activator.FindObject(type = Type.SPELL, name = name):
            return

        spell = GetArchetype("spell_" + name.replace(" ", "_")).clone

        if activator.PayAmount(spell.value):
            inf.add_msg("You pay {}.".format(CostString(spell.value)), COLOR_YELLOW)
            inf.add_msg("Pleasure doing business with you!")
            inf.add_msg_icon(spell.face[0], name, fit = True)
            activator.CreateObject(spell.arch.name)
        else:
            inf.add_msg("You don't have enough money...")

main()
inf.finish()
