from Atrinik import *
import string

activator=WhoIsActivator()
whoami=WhoAmI()
quest_arch_name = "head_ant_queen"
quest_item_name = "water well ant queen head"

msg = WhatIsMessage().strip().lower()
text = string.split(msg)
qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
item = activator.CheckInventory(1, quest_arch_name, quest_item_name)

if text[0] == 'archery':
	whoami.SayTo(activator,"\nYes, there are three archery skills:\nBow Archery is the most common firing arrows.\nSling Archery allows fast firing stones with less damage.\nCrossbow Archery uses x-bows and bolts. Slow but powerful.")

elif text[0] == 'learn':
	if qitem != None:
		whoami.SayTo(activator,"\nSorry, I can only teach you *one* archery skill.")
	else:
		whoami.SayTo(activator,"\nWell, there are three different ^archery^ skills.\nI can teach you only *ONE* of them.\nYou have to stay with it then. So choose wisely.\nI can tell you more about ^archery^. But before i teach you i have a little ^quest^ for you.");

elif msg == 'teach me bow':
	if qitem != None or item == None:
		whoami.SayTo(activator,"\nI can't ^teach^ you this now.")	
	else:
		activator.AddQuestObject(quest_arch_name, quest_item_name)
		item.Remove()
		whoami.SayTo(activator, "Here we go!")
		whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Chereth teaches some ancient skill.", COLOR_YELLOW) 
		activator.AcquireSkill(GetSkillNr('bow archery'), LEARN)	
		activator.CreateObjectInside("bow_short", 1,1)
		activator.Write("Chereth gives you a short bow.", COLOR_WHITE)
		activator.CreateObjectInside("arrow", 1,12)
		activator.Write("Chereth gives you 12 arrows.", COLOR_WHITE)

elif msg == 'teach me sling':
	if qitem != None or item == None:
		whoami.SayTo(activator,"\nI can't ^teach^ you this now.")	
	else:
		activator.AddQuestObject(quest_arch_name, quest_item_name)
		item.Remove()
		whoami.SayTo(activator, "Here we go!")
		whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Chereth teaches some ancient skill.", COLOR_YELLOW) 
		activator.AcquireSkill(GetSkillNr('sling archery'), LEARN)	
		activator.CreateObjectInside("sling_small", 1,1)
		activator.Write("Chereth gives you a small sling.", COLOR_WHITE)
		activator.CreateObjectInside("sstone", 1,12)
		activator.Write("Chereth gives you 12 sling stones.", COLOR_WHITE)

elif msg == 'teach me crossbow':
	if qitem != None or item == None:
		whoami.SayTo(activator,"\nI can't ^teach^ you this now.")	
	else:
		activator.AddQuestObject(quest_arch_name, quest_item_name)
		item.Remove()
		whoami.SayTo(activator, "Here we go!")
		whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Chereth teaches some ancient skill.", COLOR_YELLOW) 
		activator.AcquireSkill(GetSkillNr('crossbow archery'), LEARN)	
		activator.CreateObjectInside("crossbow_small", 1,1)
		activator.Write("Chereth gives you a small crossbow.", COLOR_WHITE)
		activator.CreateObjectInside("bolt", 1,12)
		activator.Write("Chereth gives you 12 bolts.", COLOR_WHITE)
		
elif text[0] == 'teach':
	if qitem != None:
		whoami.SayTo(activator,"\nSorry, I can only teach you *one* archery skill.")
	else:
		if item == None:
			whoami.SayTo(activator,"\nWhere is the queen's head? I don't see it.\nSolve the ^quest^ first and kill the ant queen.\nThen I will teach you.")
		else:
			whoami.SayTo(activator,"\nAs reward I will teach you an archery skill.\nChoose wisely. I can only teach you *one* of three skills!!\nYou want some info about the ^archery^ skills?\nIf you know your choice tell me ^teach me bow^,\n^teach me sling^ or ^teach me crossbow^.")

elif text[0] == 'quest':
	if qitem != None:
		whoami.SayTo(activator,"\nI have no quest for you after you helped us out.")
	else:
		if item == None:
			whoami.SayTo(activator,"\nYes, we need your help first.\nAs supply chief the water support of this outpost\nis under my command. We noticed last few days problems\nwith our main water source.\nIt seems a traveling hive of giant ants has invaded the\ncaverns under our water well.\nEnter the well next to this house and kill the ant queen!\nBring me her head as a trophy and I will ^teach^ you.")
		else:
			whoami.SayTo(activator,"\nThe head! You have done it!\nNow we can repair the water well.\nSay ^teach^ to me now to learn an archery skill!") 
			
elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	if qitem == None:
		if item == None:
			whoami.SayTo(activator,"\nHello, mercenary. I am Supply Chief Chereth.\nFomerly Archery Commander Chereth,\nbefore I lost my eyes.\nWell, I still know alot about ^archery^.\nPerhaps you want to ^learn^ an archery skill?")
		else:
			whoami.SayTo(activator,"\nThe head! You have done it!\nNow we can repair the water well.\nSay ^teach^ to me now to learn an archery skill!") 
	else:
		whoami.SayTo(activator,"\nHello %s.\nGood to see you back.\nI have no quest for you or your ^archery^ skill." % activator.name)
else:
	activator.Write("%s listens to you without answer." % whoami.name, COLOR_WHITE)
