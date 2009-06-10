from Atrinik import *
import string

activator=WhoIsActivator()
whoami=WhoAmI()
quest_arch_name = "tooth"
quest_item_name = "elder wyvern tooth"
qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
item = activator.CheckInventory(1, quest_arch_name, quest_item_name)
eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_PHYSICAL)

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

if text[0] == 'archery' or text[0] == 'chereth':
	whoami.SayTo(activator,"\nYou should ask Chereth about the 3 archery skills.\nShe still teaches archery and her knowledge about it is superior.");

elif text[0] == 'teach':
	skill = GetSkillNr('two-hand mastery')
	sobj = activator.GetSkill(TYPE_SKILL, skill)
	if qitem != None:
		whoami.SayTo(activator,"\nI can't teach you more.")
	else:
		if  eobj != None and eobj.level <10:
			whoami.SayTo(activator,"\nYou are not strong enough. Come back later!")
		else:
			if item == None:
				whoami.SayTo(activator,"\nBring me first the elder wyvern tooth!")
			else:
				activator.AddQuestObject(quest_arch_name, quest_item_name)
				activator.Write("Taleus takes %s from your inventory." % item.name, COLOR_WHITE)
				item.Remove()
				if sobj != None:
					whoami.SayTo(activator,"\nYou already know that skill?!")
				else:
					whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Taleus teaches some ancient skill.", COLOR_YELLOW) 
					activator.AcquireSkill(skill, LEARN)	

		
	
elif text[0] == 'two-hand':
	whoami.SayTo(activator,"\nTwo-hand mastery will allow you to fight with 2-hand weapons. You will do more damage and better hit at the cost of lower protection because you can't wield a shield.");

elif text[0] == 'elder' or text[0] == 'quest':
	if qitem == None:
		if item == None:
			whoami.SayTo(activator,"\nThe elder wyverns are the most aggressive and strongest of the wyverns in that cave.");
			if  eobj != None and eobj.level >=10:
				whoami.SayTo(activator,"Hm, it seems you are strong enough to try it...\nIf you can kill one or two i will help you too.\nI'll make a deal with you:\nBring me the tooth of an elder wyvern and I will teach you ^two-hand^ mastery.",1);
			else:
				whoami.SayTo(activator,"But they are too strong for you at the moment.\nTrain some more melee fighting.\nIf you have become stronger I will give you a quest.",1)
		else:
			whoami.SayTo(activator,"\nAh, you are back.\nYou have the tooth?\nI will ^teach^ you two-hand mastery if you are strong enough!")
	else:
		whoami.SayTo(activator,"\nYes, you have done good work in the wyvern cave.")
	
elif text[0] == 'wyverns' or text[0] == 'wyvern':
	if qitem == None:
		if item == None:
			whoami.SayTo(activator,"\nThe wyverns live in a big cave southwest of stoneglow.\nThey are dangerous and attacked us several times.\nWe have sent some expeditions but there are alot of them.\nThe biggest problems are the ^elder^ wyverns.")
			if  eobj != None and eobj.level <10:
				whoami.SayTo(activator,"Hm, your physique is not high enough.\nCome back after you get stronger and I have a quest for you!",1)
			else:
				whoami.SayTo(activator,"You are strong enough now.\nI have a ^quest^ for you.",1)
		else:
			whoami.SayTo(activator,"\nAh, you are back.\nYou have the tooth?\nThen i will ^teach^ you two-hand mastery!")
	else:
		whoami.SayTo(activator,"\nYes, you have done good work in the wyvern cave.")

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	if qitem == None:
		if item == None:
			whoami.SayTo(activator,"\nHello %s.\nI am the current ^archery^ commander after ^Chereth^\nlost her eyes in this terrible fight with the ^wyverns^." % activator.name)
		else:
			if eobj == None or eobj.level <10:
				whoami.SayTo(activator,"\nYou have a wyvern tooth???\nAmazing! When you are stronger, bring me a wyvern tooth and I will reward you!")
			else:
				whoami.SayTo(activator,"\nAh, you are back.\nYou have the tooth?\nThen I will ^teach^ you two-hand mastery!")
	else:
		whoami.SayTo(activator,"\nHello %s.\nGood to see you back." % activator.name)
	
else:
	activator.Write("%s listens to you without answer." % whoami.name, COLOR_WHITE)
