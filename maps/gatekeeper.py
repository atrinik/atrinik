import Atrinik
import string

activator=Atrinik.WhoIsActivator()
whoami=Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if activator.DoKnowSkill(Atrinik.GetSkillNr("impact weapons")) == 1 or activator.DoKnowSkill(Atrinik.GetSkillNr("slash weapons")) == 1 or activator.DoKnowSkill(Atrinik.GetSkillNr("cleave weapons")) == 1 or activator.DoKnowSkill(Atrinik.GetSkillNr("pierce weapons")) == 1:
	whoami.SayTo(activator,"You already know a weapon skill. Start playing now!" )
	activator.SetPosition(10,2)

elif msg == 'food':
	activator.SetFood(999)
	whoami.SayTo(activator,'\nYour stomach is filled again.')

elif msg == 'slash':
	whoami.SayTo(activator,"\nDone! Now enter the magic exit. Good luck!" )

	skill = Atrinik.GetSkillNr('slash weapons')
	if skill == -1:
		whoami.SayTo(activator,"Unknown skill." )
	else:
		if activator.DoKnowSkill(skill) == 1:
			whoami.SayTo(activator,"You already know this skill." )
		else:
			activator.Write("the gatekeeper gives you a sword.", 0)
			activator.AcquireSkill(skill, Atrinik.LEARN)
			activator.Apply(activator.CreateObjectInside("shortsword", 1,1,1),0)
	activator.SetPosition(10,2)
elif msg == 'impact':
	whoami.SayTo(activator,"\nDone! Now enter the magic exit. Good luck!" )

	skill = Atrinik.GetSkillNr('impact weapons')
	if skill == -1:
		whoami.SayTo(activator,"Unknown skill." )
	else:
		if activator.DoKnowSkill(skill) == 1:
			whoami.SayTo(activator,"You already know this skill." )
		else:
			activator.Write("the gatekeeper gives you a small morningstar.", 0)
			activator.AcquireSkill(skill, 0)
			activator.Apply(activator.CreateObjectInside("mstar_small", 1,1,1),0)
	activator.SetPosition(10,2)
elif msg == 'cleave':
	whoami.SayTo(activator,"\nDone! Now enter the magic exit. Good luck!" )

	skill = Atrinik.GetSkillNr('cleave weapons')
	if skill == -1:
		whoami.SayTo(activator,"Unknown skill." )
	else:
		if activator.DoKnowSkill(skill) == 1:
			whoami.SayTo(activator,"You already know this skill." )
		else:
			activator.Write("the gatekeeper gives you a axe.", 0)
			activator.AcquireSkill(skill, 0)
			activator.Apply(activator.CreateObjectInside("axe_small", 1,1,1),0)
	activator.SetPosition(10,2)
elif msg == 'pierce':
	whoami.SayTo(activator,"\nDone! Now enter the magic exit. Good luck!" )

	skill = Atrinik.GetSkillNr('pierce weapons')
	if skill == -1:
		whoami.SayTo(activator,"Unknown skill." )
	else:
		if activator.DoKnowSkill(skill) == 1:
			whoami.SayTo(activator,"You already know this skill." )
		else:
			activator.Write("the gatekeeper gives you a large dagger.", 0)
			activator.AcquireSkill(skill, 0)
			activator.Apply(activator.CreateObjectInside("dagger_large", 1,1,1),0)
	activator.SetPosition(10,2)
elif msg == 'weapons':
	whoami.SayTo(activator,
        "\nWe have 4 different weapon skills.\n"
        "Each skill allow the use of a special kind of weapons.\n"
        "Slash weapons are swords.\n"
        "Cleave weapons are axe-like weapons.\n"
        "Pierce weapons are rapiers and daggers.\n"
        "Impact weapons are maces or hammers.\n"
        "Note that you can learn all those skills later somewhere.\n"
        "Now select one and tell me: ^slash^, ^cleave^, ^pierce^ or ^impact^?")
elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	whoami.SayTo(activator,
        "\nHello! I am the Gatekeeper.\n"
        "I will give you your start weapon skill and your\n"
        "first weapon. Then I will teleport you to the last part of the tutorial.\n"
        "But first tell me which weapon skill you want.\n"
        "You can select between slash weapons, cleave weapons, pierce weapons or impact weapons.\n"
        "Ask me about ^weapons^ to hear more!\n")
else:
	activator.Write("the gatekeeper seems not to notice you.\nYou should try ^hello^, ^hi^ or ^hey^...", 0)
