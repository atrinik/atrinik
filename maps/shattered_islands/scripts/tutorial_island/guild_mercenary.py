## @file
## Initial quest for the player on Tutorial Island.
##
## The player must find Cashin's helm and return with it to Cashin, then
## he will be able to join the Mercenary guild and receive more quests.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
text = msg.split()

exec(open(CreatePathname("/shattered_islands/scripts/tutorial_island/quests.py")).read())

## Initialize QuestManager.
qm = QuestManager(activator, quest_items["mercenary_guild"]["info"])

def main():
	## Get the guild force of the player.
	guild_force = activator.GetGuildForce()

	## Mercenary guild tag.
	guild_tag = "Mercenary"
	## Initial guild rank.
	guild_rank = ""

	if msg == "hello" or msg == "hi" or msg == "hey":
		if guild_force.slaying == guild_tag:
			me.SayTo(activator, "\nHello {0}! Welcome back.\nNice that you have joined our <a>troops</a>.\nAsk me when you need info about the guild.".format(activator.name))
		else:
			if not qm.started():
				me.SayTo(activator, "\nHello, I am the mercenary guildmaster.\nSay <a>guild</a> if you want to join or for info about the guild.\nOur task is to support the regular <a>troops</a>.\nIf you want a good start - we will give it to you.")
			else:
				pinfo = activator.GetPlayerInfo("GUILD_INFO")

				while pinfo != None:
					if pinfo.slaying == guild_tag:
						me.SayTo(activator, "\nYou are an old member of ours!\nYou can re<a>join</a> at any time.\nYou will get your old guild rank back too.")
						guild_rank = pinfo.title

						if guild_rank == None:
							guild_rank = ""
						break

					pinfo = activator.GetNextPlayerInfo(pinfo)

				if pinfo == None or pinfo.slaying != guild_tag:
					if qm.finished():
						me.SayTo(activator, "\nAh, you have my helmet! Excellent!\nWell, keep it and may it protect you.\nIf you want you can <a>join</a> us now.")
					else:
						me.SayTo(activator, "\nI don't see my helmet... Where is it?\nRemember: Enter the hole next to me and kill the ants there.\nOne of them stole my old helmet.\nBring it back to me and I will let you join the <a>guild</a>.")

	elif text[0] == "join":
		if not qm.started():
			me.SayTo(activator, "\nBefore you can join you must do a quest. Say <a>guild</a> for more information.")
		elif guild_force.slaying == guild_tag:
			me.SayTo(activator, "\nYou are a Mercenary, aren't you?\nYou can't join twice...")
		else:
			## So request a player_info arch set to GUILD_INFO
			pinfo = activator.GetPlayerInfo("GUILD_INFO")

			# If none or not what we want, go to else... but first search
			while pinfo != None:
				if pinfo.slaying == guild_tag:
					me.SayTo(activator, "\nWelcome old member!\nYou rejoin now with your old guild rank.")
					guild_rank = pinfo.title

					if guild_rank == None:
						guild_rank = ""

					break

				pinfo = activator.GetNextPlayerInfo(pinfo)

			if pinfo == None or pinfo.slaying != guild_tag:
				if not qm.finished():
					me.SayTo(activator, "\nI don't see my helmet... Where is it?\nRemember: Enter the hole next to me and kill the ants there.\nOne of them stole my old helmet.\nBring it back to me and I will let you join the <a>guild</a>.")
				else:
					qm.complete()

					me.SayTo(activator, "\nThere is my helmet back... Well, keep it!\nI am impressed. I think you will make your way.\nWelcome to the Mercenaries of Thraal!\nAsk Jahrlen for instructions. He is down the stairs.")

					## Setup an own PLAYER_INFO
					guild_info = activator.CreatePlayerInfo("GUILD_INFO")

					# Title in this guild
					guild_info.title = guild_rank
					# The guild_info tag of this guild
					guild_info.slaying = guild_tag
					# Our active guild, give us our profession title
					activator.SetGuildForce(guild_rank)
					# Real tag to the guild_info
					guild_force.slaying = guild_tag
			else:
				# Our active guild, give us our profession title
				activator.SetGuildForce(guild_rank)
				# The real tag to the guild_info
				guild_force.slaying = guild_tag

	elif text[0] == "leave":
		if guild_force.slaying != guild_tag:
			me.SayTo(activator, "\nYou are not a member here...")
		else:
			me.SayTo(activator, "\nOk, you are out!\nYou can rejoin any time.")

			# Empty string forces the system to insert a NULL in the object
			guild_force = activator.SetGuildForce("")
			guild_force.slaying = ""

	elif text[0] == "accept":
		if not qm.started():
			qm.start()
			me.SayTo(activator, "\nRemember, bring me back my old helmet and you can join the <a>guild</a>!")
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping us out.")

	elif text[0] == "guild":
		if guild_force.slaying == guild_tag:
			me.SayTo(activator, "\nYou can <a>leave</a> at any time.\nYou can also rejoin without problems.")
		else:
			if not qm.started():
				me.SayTo(activator, "\nBefore you can <a>join</a> the guild I have a small task for you.\nSome giant ants have invaded our water supply.\nSee this hole by my side!\nOne of those silly ants stole my old helmet!\nEnter the hole and kill the ants there.\nNo fear, they are weak.\nBring me the helmet back and I will let you <a>join</a>.\nDo you <a>accept</a> this quest?")
			else:
				pinfo = activator.GetPlayerInfo("GUILD_INFO")

				while pinfo != None:
					if pinfo.slaying == guild_tag:
						me.SayTo(activator, "\nYou are an old member of ours!\nYou can re<a>join</a> at any time.\nYou will get your old guild rank back too.")
						guild_rank = pinfo.title

						if guild_rank == None:
							guild_rank = ""
						break

					pinfo = activator.GetNextPlayerInfo(pinfo)

				if pinfo == None or pinfo.slaying != guild_tag:
					if qm.finished():
						me.SayTo(activator, "\nAh, you have my helmet! Excellent!\nWell, keep it and may it protect you.\nIf you want you can <a>join</a> us now.")
					else:
						me.SayTo(activator, "\nI don't see my helmet... Where is it?\nRemember: Enter the hole next to me and kill the ants there.\nOne of them stole my old helmet.\nBring it back to me and I will let you join the <a>guild</a>.")

	elif text[0] == "jahrlen":
		me.SayTo(activator, "\nJahrlen is our guild mage.\nWell, normally we don't have a guild mage.\nBut we are at war here and he was assigned to us.\nIn fact, he is a high level chronomancer and we are honored he helps us.\nHe is in our guild rooms. Talk to him when you meet him!\nHe often has tasks and quests for newbies.")

	elif text[0] == "troops":
		me.SayTo(activator, "\nWe, as part of the the Thraal army corps, are invading these abandoned areas after the defeat of Moroch.\nWell, the chronomancers ensured us after they created the portal that we are still in the galactic main sphere.\nBut it seems to me that these lands have many wormholes to other places...\nPerhaps the long time under Morochs influence has weakened the borders between the planes. You should ask <a>Jahrlen</a> about it.")

main()
