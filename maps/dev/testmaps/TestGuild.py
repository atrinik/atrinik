import Atrinik
import string

activator=Atrinik.WhoIsActivator()
whoami=Atrinik.WhoAmI()
guild_tag = "Guild of Tests"
guild_rank = ""

msg = Atrinik.WhatIsMessage().strip().lower()
text = string.split(msg)

# Thats the classic join procedure.
# Look first to our active guild_force. Don't change anything when we already in & active
# If not, look for a old GUILD_INFO. When we was in the past a member and have leaved, just
# rejoin with old rank. There *can* be other action in this case of course.
# Perhaps the Assassin guild don't like members which have leaved in the past...
# if we are all new, just create a guild_info and set it up.
if text[0] == "join":
        # check guild_force for active guild first.
        # here we can trigger different action, perhaps when we come as evil mage
        # in a paladin guild...
	# there should be always a guild_force, so we don't check here after
	guild_force = activator.GetGuildForce()
	if guild_force.slaying == guild_tag :
		whoami.SayTo(activator, "\nYou are in Testguild, aren't you?!!\nYou can't join twice...")
        else :
		#start query PLAYER_INFO (GUILD_INFO) and test it
                # remember that a player_info is a container for different things
                # so request a player_info arch set to GUILD_INFO
		pinfo = activator.GetPlayerInfo("GUILD_INFO")
		while pinfo != None: # if none or not what we want, go to else... but first search
			if pinfo.slaying == guild_tag:
				whoami.SayTo(activator, "\nYou are a old member of us!\nI let you rejoin.\nYou get your old guild rank back.")
				guild_rank = pinfo.title
				if guild_rank == None:
					guild_rank = ""
				break
			pinfo = activator.GetNextPlayerInfo(pinfo)

		if pinfo == None or pinfo.slaying != guild_tag:
			whoami.SayTo(activator, "\nWelcome to the Test Guild.\nYou are a new member now.\n")
			# setup a own PLAYER_INFO (name GUILD_INFO)
			guild_info = activator.CreatePlayerInfo("GUILD_INFO")
			guild_info.title = guild_rank # thats our title in this guild
			guild_info.slaying = guild_tag # thats the guild_info tag of this guild
		# Ok, we have all we want, setup the GUILD_FORCE now and make it "official"
                # Using setGuildForce is a must because this routine setup the internals too
                # (like triggering update to the client, etc)
		activator.SetGuildForce(guild_rank)  # Our active guild, give us our profession title 
		guild_force.slaying = guild_tag      # thats the real tag to the guild_info

# thats a test for changing something in the guild_info. It works like this:
# 1. we must check this guild is active - get the guild_force slaying entry 
# 1a. if tag is from this guild, we try to catch the guild_info. This SHOULD be always possible,
#     but when not we give out a warning message.
#     If we find the guild_info, we set guild_info & guild_force to the new title.
# 1b. if the guild_force is not set to this guild, we are not active or had never joined.
#     Then always rejoin first.
elif text[0] == "advance":
		guild_force = activator.GetGuildForce()
		if guild_force.slaying == guild_tag :
			pinfo = activator.GetPlayerInfo("GUILD_INFO")
			while pinfo != None: #go through all player_info 
				if pinfo.slaying == guild_tag:
					whoami.SayTo(activator, "You are now a master of the Test Guild")
					new_guild_rank = "Tester"
					pinfo.title = new_guild_rank
					activator.SetGuildForce(new_guild_rank) 
					break
				pinfo = activator.GetNextPlayerInfo(pinfo)
			else:
				whoami.SayTo(activator, "Hmmmm..., your guild_force is from us, but we can't find the guild_info?\nThere has someone messed up this script!")
		else:
			whoami.SayTo(activator, "Ha, you should first join us!")
			
elif text[0] == "leave":
		whoami.SayTo(activator, "Ok, you are out!\nYou can rejoin every time.")
		guild_force = activator.SetGuildForce("") # "" forces the routine to insert a NULL in the obj...
		guild_force.slaying = ""

elif Atrinik.WhatIsMessage() == 'sir':
	whoami.SayTo(activator, 'Your rank is now Sir!')
	activator.SetRank("Sir")

elif Atrinik.WhatIsMessage() == 'lord':
	whoami.SayTo(activator, 'Your rank is now Lord!')
	activator.SetRank("Lord")

else:
		whoami.SayTo(activator,"\nI am the Guildmaster of the Test Guild.\nSay ^join^ to join the guild.\nYou can say ^leave^ when you want leave the guild or ^advance^ to get a guild title.\nYou can get ranks when you type ^sir^ or ^lord^.")
