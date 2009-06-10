from Atrinik import *
import string

activator=WhoIsActivator()
whoami=WhoAmI()
quest_arch_name = "bracers_ac"
quest_item_name = "crystalized slime master bracers"

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

if msg == "source":
	qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
	if qitem == None:
		whoami.Communicate("/spit")
		whoami.SayTo(activator,"\nWell, these slimes are not really dangerous.\nThey are weak and we can slay them easily.\nBut we must handle them so they can't invade the city.\nMy troopers are bound here and so we can't help\nat more important places.\nWe need to kill the ^master slime^ to stop this." )
	else:
		whoami.SayTo(activator,"\nYou have done it! You want to learn ^remove traps^?")

elif msg == "master slime" or msg == "master":
	qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
	if qitem == None:
		whoami.SayTo(activator,"\nYes, there must be a cave or something in this area.\nWe found some entries in the southeast.\nThere must be the master slime.\nHm... If you can kill him I can teach you ^remove traps^!\nIts really a most important skill in these lands.")
	else:
		whoami.SayTo(activator,"\nYou have done it! You want to learn ^remove traps^?")

elif msg == "remove traps":
	qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
	if qitem == None:
		whoami.SayTo(activator,"\nFirst kill the master slime!\nShow me a part of his body as proof!");
	else:
		skill = GetSkillNr('remove traps')
		if skill == -1:
			whoami.SayTo(activator,"Unknown skill - find traps." )
		else:
			sobj = activator.GetSkill(TYPE_SKILL, skill)
			if sobj == None:
				whoami.SayTo(activator,"\nSay ^teach me remove traps^ for the skill.")
			else:
				slevel = sobj.level + 1
				eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_AGILITY)
				if  eobj != None and eobj.level >= slevel:
					amount = slevel * slevel * (50+slevel) * 3;
					whoami.SayTo(activator,"Remove traps lvl %d will cost you\n%s." % (slevel, activator.ShowCost(amount)))
					whoami.SayTo(activator,"Say to me ^teach me remove traps^ for teaching!",1 )
				else:
					whoami.SayTo(activator,"Ho, yo agility is to low to teach!!" )


elif msg == "teach me remove traps":
	qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
	if qitem == None:
		whoami.SayTo(activator,"\nFirst kill the master slime!\nShow me a part of his body as proof!");
	else:
		skill = GetSkillNr('remove traps')
		if skill == -1:
			whoami.SayTo(activator,"Unknown skill - find traps." )
		else:
			sobj = activator.GetSkill(TYPE_SKILL, skill)
			if sobj == None:
				whoami.SayTo(activator, "Here we go!")
				whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Regulus teaches some ancient skill.", COLOR_YELLOW) 
				activator.AcquireSkill(skill, LEARN)	
			else:
				slevel = sobj.level + 1
				eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_AGILITY)
				if  eobj == None or eobj.level < slevel:
					whoami.SayTo(activator,"Ho, yo agility to low to teach!!" )
				else:
					amount = slevel * slevel * (50+slevel) * 3;
					if activator.PayAmount(amount) == 1:
						activator.Write("You pay Regulus %s." % activator.ShowCost(amount))
						whoami.SayTo(activator, "Here we go!")
						whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Regulus teaches some ancient skill.", COLOR_YELLOW) 
						activator.SetSkill(TYPE_SKILL, skill, slevel, 0)
					else:
						whoami.SayTo(activator,"Sorry, you have not enough money." )

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
	if qitem == None:
		whoami.SayTo(activator,"\nCAREFUL TROOPERS! INCOMING FROM SOUTHWEST!\nSorry, we are under heavy attack today again.\nWe slay them in hundreds but they breed fast.\nWe really need to find the ^source^ of this attack.")
	else:
		skill = GetSkillNr('remove traps')
		if skill == -1:
			whoami.SayTo(activator,"Unknown skill - find traps." )
		else:
			sobj = activator.GetSkill(TYPE_SKILL, skill)
			if sobj == None:
				whoami.SayTo(activator,"\nYou have done it! Really great!\nWe noticed your success because the attacking\nwaves are decreasing now in numbers every minute.\nWe will eliminate them in some days I think.\nNow say ^teach me remove traps^ for the reward.")
			else:
				slevel = sobj.level + 1
				eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_AGILITY)
				if  eobj != None and eobj.level >= slevel:
					amount = slevel * slevel * (50+slevel) * 3;
					whoami.SayTo(activator,"You want to learn more, yes?\nRemove traps lvl %d will cost you\n%s." % (slevel, activator.ShowCost(amount)))
					whoami.SayTo(activator,"Say to me ^teach me remove traps^ for teaching!",1 )
				else:
					whoami.SayTo(activator,"Ho, yo agility is too low to teach!" )
else:
	activator.Write("%s listens to you without answer." % whoami.name, COLOR_WHITE)
