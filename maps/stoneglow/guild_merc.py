from Atrinik import *
import string

activator=WhoIsActivator()
whoami=WhoAmI()
guild_tag = "Mercenary"
guild_rank = ""
quest_arch_name = "helm_leather"
quest_item_name = "Cashin's leather cap"

msg = WhatIsMessage().strip().lower()
text = string.split(msg)
guild_force = activator.GetGuildForce()

if text[0] == "join":
	if guild_force.slaying == guild_tag :
		whoami.SayTo(activator, "\nYou are a Mercenary, aren't you?!\nYou can't join twice...")
	else :
                # so request a player_info arch set to GUILD_INFO
		pinfo = activator.GetPlayerInfo("GUILD_INFO")
		while pinfo != None: # if none or not what we want, go to else... but first search
			if pinfo.slaying == guild_tag:
				whoami.SayTo(activator, "\nWelcome old member!\nYou rejoin now with your old guild rank.")
				guild_rank = pinfo.title
				if guild_rank == None:
					guild_rank = ""
				break
			pinfo = activator.GetNextPlayerInfo(pinfo)
			
		if pinfo == None or pinfo.slaying != guild_tag:
			qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
			if qitem == None:
				whoami.SayTo(activator, "\nI don't see my helm... Where is it?\nRemember: Enter the hole next to me\nand kill the ants there.\nOne of them has my old helm stolen.\nBring it back to me and I will let you join the ^guild^.")
			else:
				whoami.SayTo(activator, "\nThere is my helm back... Well, keep it!\n I am impressed. I think you will make your way.\nWelcome to the Mercenaries of Thraal!\nAsk Jahrlen for instructions. He is down the stairs.")
				# setup a own PLAYER_INFO (name GUILD_INFO)
				guild_info = activator.CreatePlayerInfo("GUILD_INFO")
				guild_info.title = guild_rank # thats our title in this guild
				guild_info.slaying = guild_tag # thats the guild_info tag of this guild
				activator.SetGuildForce(guild_rank)  # Our active guild, give us our profession title 
				guild_force.slaying = guild_tag # thats the real tag to the guild_info
				
		else:
			activator.SetGuildForce(guild_rank)  # Our active guild, give us our profession title 
			guild_force.slaying = guild_tag # thats the real tag to the guild_info

			
elif text[0] == "leave":
	if guild_force.slaying != guild_tag :
		whoami.SayTo(activator, "\nYou are not a member here...")
	else:
		whoami.SayTo(activator, "\nOk, you are out!\nYou can rejoin any time.")
		guild_force = activator.SetGuildForce("") # "" forces the routine to insert a NULL in the obj...
		guild_force.slaying = ""

elif text[0] == "guild":
	if guild_force.slaying == guild_tag :
		whoami.SayTo(activator,"\nYou can ^leave^ at any time.\nYou can also rejoin without problems.")
	else:
		qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
		if qitem == None:
			whoami.SayTo(activator,"\nBefore you can ^join^ the guild I have a small task for you.\nSome giant ants have invaded our water supply.\nSee this hole on my side!\nOne of those silly ants has stolen my old helmet!\nEnter the hole and kill the ants there!\nNo fear, they are weak.\nBring me the helm back and I will let you ^join^.\n");		
		else:
			pinfo = activator.GetPlayerInfo("GUILD_INFO")
			while pinfo != None:
				if pinfo.slaying == guild_tag:
					whoami.SayTo(activator, "\nYou are an old member of ours!\nYou can re^join^ at any time.\nYou will get your old guild rank back too.")
					guild_rank = pinfo.title
					if guild_rank == None:
						guild_rank = ""
					break
				pinfo = activator.GetNextPlayerInfo(pinfo)
			if pinfo == None or pinfo.slaying != guild_tag:
				whoami.SayTo(activator,"\nAh, you have my helm! Excellent!\nWell, keep it and may it protect you.\nIf you want you can ^join^ us now.")

elif text[0] == "jahrlen":
		whoami.SayTo(activator,"\nJahrlen is our guild mage.\nWell, normally we don't have a guild mage.\nBut we are at war here and he was assigned to us.\nIn fact, he is a high level chronomancer and we are honored he helps us.\nHe is in our guild rooms. Talk to him when you meet him!\nHe often has tasks and quests for newbies.")

elif text[0] == "troops":
		whoami.SayTo(activator,"\nWe, as part of the the Thraal army corps, are invading\nthese abandomed areas after the defeat of Moroch.\nWell, the chronomancers ensured us after they created\nthe portal that we are still in the galactic main sphere.\nBut it seems to me that these lands have many wormholes\nto other places...\nPerhaps the long time under Morochs influence has weakened the borders between the planes. You should ask ^Jahrlen^ about it.");

elif msg == 'hello' or msg == 'hi' or msg == 'hey':
	if guild_force.slaying == guild_tag :
		whoami.SayTo(activator, "\nHello %s! Welcome back.\nNice that you have joined our ^troops^.\nAsk me when you need info about the guild." % activator.name)
	else:
		# cap is one time drop - if we got it this is auto set
		qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
		if qitem == None:
			whoami.SayTo(activator,"\nHello, I am the mercenary guildmaster.\nSay ^guild^ if you want to join or for info about the guild.\nOur task is to support the regular ^troops^.\nIf you want a good start - we will give it you.")
		else:
			pinfo = activator.GetPlayerInfo("GUILD_INFO")
			while pinfo != None:
				if pinfo.slaying == guild_tag:
					whoami.SayTo(activator, "\nYou are an old member of ours!\nYou can re^join^ at any time.\nYou will get your old guild rank back too.")
					guild_rank = pinfo.title
					if guild_rank == None:
						guild_rank = ""
					break
				pinfo = activator.GetNextPlayerInfo(pinfo)
			if pinfo == None or pinfo.slaying != guild_tag:
				whoami.SayTo(activator,"\nAh, you have my helm! Excellent!\nWell, keep it and may it protect you.\nIf you want you can ^join^ us now.")
else:
	activator.Write("%s listens to you without answer." % whoami.name, COLOR_WHITE)
