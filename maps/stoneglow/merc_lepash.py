from Atrinik import *
import string

activator=WhoIsActivator()
whoami=WhoAmI()
quest_arch_name = "horn"
quest_item_name = "clan horn of the hill giants"
qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
item = activator.CheckInventory(1, quest_arch_name, quest_item_name)
eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_PHYSICAL)

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

if text[0] == 'home':
	whoami.SayTo(activator,"\nYes, we still have heavy logistic problems.\nWe have not half the men we need to control half of the area we should.\nWe have not enough supply and not enough rooms.\nWell, means all is normal when you do such a heavy invasion like we do at this moment.");

elif text[0] == 'teach':
	skill = GetSkillNr('polearm mastery')
	sobj = activator.GetSkill(TYPE_SKILL, skill)
	if qitem != None:
		whoami.SayTo(activator,"\nI can't teach you more.")
	else:
		if  eobj != None and eobj.level <11:
			whoami.SayTo(activator,"\nYour level is to low. Come back later!")
		else:
			if item == None:
				whoami.SayTo(activator,"\nBring me first the clan horn of the hill giants!")
			else:
				activator.AddQuestObject(quest_arch_name, quest_item_name)
				activator.Write("Lepash takes %s from your inventory." % item.name, COLOR_WHITE)
				item.Remove()
				if sobj != None:
					whoami.SayTo(activator,"\nYou already know that skill?!")
				else:
					whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Lepash teaches some ancient skill.", COLOR_YELLOW) 
					activator.AcquireSkill(skill, LEARN)	

		
	
elif text[0] == 'polearm' or text[0] == 'polearms':
	whoami.SayTo(activator,"\nPolearm mastery will allow you to fight with polearm weapons. You will do alot more damage and you will have some protection even though you can't wear a shield using polearms.");

elif text[0] == 'job':
	if qitem == None:
		if item == None:
			whoami.SayTo(activator,"\nThere is somewhere north of stoneglow a hill giant camp.\nPerhaps in a cave or something.\nWe noticed them around stoneglow.")
			if  eobj != None and eobj.level <11:
				whoami.SayTo(activator,"Hm, your physique is not high enough.\nCome back after you get stronger and I'll give you more information!",1)
			else:
				whoami.SayTo(activator,"You are strong enough.\nFind this hill giant camp and kill the camp leader.\nHe should have a sign of power like a clan horn or something. Show it to me and i will teach you ^polearm^ mastery",1)
		else:
			whoami.SayTo(activator,"\nAh, you are back.\nYou have the clan horn?\nThen I will ^teach^ you polearm mastery!")
	else:
		whoami.SayTo(activator,"\nYes, you have done good work with the hill giants.\n")

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	if qitem == None:
		if item == None:
			whoami.SayTo(activator,"\nHello there. I am guard commander Lepash.\nI have taken ^home^ here in this mercenary guild.\nHm, you are interested in a ^job^?")
		else:
			if eobj == None or eobj.level <11:
				whoami.SayTo(activator,"\nYou have the clan horn???\nAmazing! When you are higher in level show me the horn again and I will reward you!")
			else:
				whoami.SayTo(activator,"\nAh, you are back.\nYou have the clan horn?\nThen I will ^teach^ you polearm mastery!")
	else:
		whoami.SayTo(activator,"\nHello %s.\nGood to see you back." % activator.name)
	
else:
	activator.Write("%s listens to you without answer." % whoami.name, COLOR_WHITE)
