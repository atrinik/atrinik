from Atrinik import *
import string

activator=WhoIsActivator()
whoami=WhoAmI()
quest_arch_name = "letter"
quest_item_name = "Frah'aks letter"

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

if msg == "still there, frah'ak?":
	whoami.Communicate("/spit " + activator.name)
elif text[0] == "warrior":
	whoami.SayTo(activator,"Me big chief. Me ogre destroy you.\nStomp on. Dragon kakka." )
elif text[0] == "kobolds":
	whoami.SayTo(activator,"\nKobolds traitors!\nGive gold for note, kobolds don't bring note to ogres.\nMe tell you: Kill kobold chief!\nMe will teach you ^find traps^ skill!\nShow me note i will teach you.\nKobolds in hole next room. Secret entry in wall." )
elif msg == "teach me find traps":
	skill = GetSkillNr('find traps')
	if skill == -1:
		whoami.SayTo(activator,"Unknown skill - find traps." )
	else:
		sobj = activator.GetSkill(TYPE_SKILL, skill)
		if sobj == None:
			qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
			item = activator.CheckInventory(1, quest_arch_name, quest_item_name)
			if qitem == None and item != None:
				activator.AddQuestObject(quest_arch_name, quest_item_name)
				activator.Write("%s takes %s from your inventory." % (whoami.name, item.name), COLOR_WHITE)
				item.Remove()
				whoami.SayTo(activator, "Here we go!")
				whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Frah'ak teach some ancient skill.", COLOR_YELLOW) 
				activator.AcquireSkill(skill, LEARN)	
			else:
				whoami.SayTo(activator,"\nNah, bring Frah'ak note from ^kobolds^ first!")
		else:	
			slevel = sobj.level + 1
			eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_AGILITY)
			if  eobj == None or eobj.level < slevel:
				whoami.SayTo(activator,"Ho, yo agility too low to teach!!" )
			else:
				amount = slevel * slevel * (50+slevel) * 3;
				if activator.PayAmount(amount) == 1:
					activator.Write("You pay Frah'ak %s." % activator.ShowCost(amount))
					whoami.SayTo(activator, "Here we go!")
					whoami.map.Message(whoami.x, whoami.y, MAP_INFO_NORMAL, "Frah'ak teach some ancient skill.", COLOR_YELLOW) 
					activator.SetSkill(TYPE_SKILL, skill, slevel, 0)
				else:
					whoami.SayTo(activator,"Ho, yo not enough money to pay Frah'ak!!" )

elif msg == "find traps":
	skill = GetSkillNr('find traps')
	if skill == -1:
		whoami.SayTo(activator,"Unknown skill - find traps." )
	else:
		sobj = activator.GetSkill(TYPE_SKILL, skill)
		if sobj == None:
			qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
			item = activator.CheckInventory(1, quest_arch_name, quest_item_name)
			if qitem == None and item != None:
				whoami.SayTo(activator, "\nFrah'ak tell yo truth!\n Say ^teach me find traps^ now!")
			else:
				whoami.SayTo(activator,"\nNah, bring Frah'ak note from ^kobolds^ first!")
		else:	
			slevel = sobj.level + 1
			eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_AGILITY)
			if  eobj != None and eobj.level >= slevel:
				whoami.SayTo(activator,"Find traps lvl %d will cost you\n%s." % (slevel, activator.ShowCost( slevel * slevel * (50+slevel) * 3) ) )
				whoami.SayTo(activator,"Say to me ^teach me find traps^ for teaching!!",1 )
			else:
				whoami.SayTo(activator,"Ho, yo agility to low to teach!!" )

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
	item = activator.CheckInventory(1, quest_arch_name, quest_item_name)
	skill = GetSkillNr('find traps')
	if qitem == None and item != None and skill != -1 and activator.DoKnowSkill(skill) != 1:
		whoami.Communicate("/grin " + activator.name)
		whoami.SayTo(activator,"\nAshahk! Yo bring me note!\nKobold chief bad time now, ha?\nNow me will teach you!\nSay ^teach me find traps^ now!")		
	else:
		if qitem != None:
			whoami.SayTo(activator,"\nAshahk! Yo want me teaching yo more ^find traps^?\nWill teach for money.")		
		else:
			whoami.SayTo(activator,"\nYo shut up.\nYo grack zhal hihzuk alshzu...\nMe mighty ogre chief.\nMe ^warrior^ will destroy yo. They come.\nGuard and ^Kobolds^ will die then.")

else:
	activator.Write("%s listens to you without answer." % whoami.name, COLOR_WHITE)
