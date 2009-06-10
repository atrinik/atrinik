import Atrinik
import string

activator=Atrinik.WhoIsActivator()
whoami=Atrinik.WhoAmI()
guild_tag = "Mercenary"

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

if text[0] == 'probe':
		guild_force = activator.GetGuildForce()
		if guild_force.slaying != guild_tag :
			whoami.SayTo(activator, "\nSorry, I can only teach active guild members.")
		else:
			whoami.SayTo(activator, 'So it be! Now you learn the spell probe!')

			skill = Atrinik.GetSkillNr('wizardry spells')
			if skill == -1:
				whoami.SayTo(activator,"Unknown skill." )
			else:
				if activator.DoKnowSkill(skill) != 1:
					whoami.SayTo(activator,"First you need this skill to cast spells." )
					activator.AcquireSkill(skill, Atrinik.LEARN)			
						
			spell = Atrinik.GetSpellNr('probe')
			if spell == -1:
				whoami.SayTo(activator,"Unknown spell." )
			else:
				if activator.DoKnowSpell(spell) == 1:
					whoami.SayTo(activator,"You already know magic probe?!" )
				else:	
					activator.AcquireSpell(spell, Atrinik.LEARN)

			whoami.SayTo(activator,"\nOk, you are ready. Perhaps you want to try the spell?\nYou can safely cast the probe on me to practice.\nJust select the spell and invoke it in my direction.") 

elif msg == 'bullet':

	spell = Atrinik.GetSpellNr('magic bullet')
	if spell == -1:
		whoami.SayTo(activator,"Unknown spell." )
	else:
		if activator.DoKnowSpell(spell) == 1:
			whoami.SayTo(activator,"You already know this spell..." )
		else:	
			if activator.PayAmount(200) == 1:
				whoami.SayTo(activator,"\nOk, I will teach you now magic bullet for 2s.")
				activator.Write( "You pay the money.", 0)
				activator.AcquireSpell(spell, Atrinik.LEARN )
			else:
				whoami.SayTo(activator,"\nSorry, you have not enough money.")

elif text[0] == 'free':
		whoami.SayTo(activator,"\nI can teach you the spell 'probe'.\nProbe is one of the most useful info spells you can learn!\nCast on unknown creatures it will grant you\ninformation about their powers and weaknesses.\nThe spell itself is very safe.\nCreatures will not notice that they were probed.\nThey will not get angry or attack.\nSay ^probe^, then I will teach you the probe spell.\nIf you don't have the needed wizardry skill,\nI will teach it to you too.")

elif text[0] == 'reward':
		whoami.SayTo(activator,"\nI will give a good item from my supply stock.");

elif text[0] == 'chronomancer':
		whoami.SayTo(activator,"\nYes, I am a master of the chronomancers of Thraal.\nWe are one of the more powerful wizard guilds.\nPerhaps, if you are higher in level and stronger...\nHmm... If you ever meet Rangaron in your life...\nthen tell him that Jahrlen has sent you.\nDon't ask me more now.")

elif text[0] == 'rangaron':

		whoami.SayTo(activator,"\nI said 'Don't ask me more now'!\nYou have problems with your ears??\nYou will notice when you meet Rangaron,\n then tell him what I told you.")

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
		whoami.SayTo(activator,"\nHello, I am Jahrlen,\nWar ^Chronomancer^ of Thraal.\nI can teach you the art of magic and your first spells.\nI'll teach you probe for ^free^ and magic ^bullet^ for 2s.")
