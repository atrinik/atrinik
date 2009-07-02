import Atrinik
import string

activator=Atrinik.WhoIsActivator()
whoami=Atrinik.WhoAmI()

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if text[0] == 'learn':
	skill = Atrinik.GetSkillNr(msg[5:].strip())
	if skill == -1:
		whoami.SayTo(activator,"Unknown skill." )
	else:
		if activator.DoKnowSkill(skill) == 1:
			whoami.SayTo(activator,"You already learned this skill." )
		else:	
			activator.AcquireSkill(skill, Atrinik.LEARN)

elif msg == 'food':
	activator.food = 999
	whoami.SayTo(activator,'\nYour stomach is filled again.')
else:
	whoami.SayTo(activator,"\nI am the Skillgiver.\nSay ^learn <skillname>^ or ^unlearn <skillname>^ (Note: unlearn skills is not implemented atm - the ideas of the daimonin skill system is to collect the skills - and don't lose them).")
