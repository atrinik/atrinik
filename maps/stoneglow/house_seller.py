from Atrinik import * 
import string

activator = WhoIsActivator()
whoami = WhoAmI()

sglow_app_tag = "SGLOW_APP_INFO"
appid_cheap = "cheap"
appid_normal = "normal"
appid_expensive = "expensive"
appid_luxurious = "luxurious"
ap_1 = "/stoneglow/appartment_1"
ap_2 = "/stoneglow/appartment_2"
ap_3 = "/stoneglow/appartment_3"
ap_4 = "/stoneglow/appartment_4"

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

pinfo = activator.GetPlayerInfo(sglow_app_tag)

def update_ap(ap_old, ap_new, pid, x, y):
	activator.Write("You pay the money.", 0)
	activator.Write("Darlin is casting some strange magic.", 0)

	if activator.SwapApartments(ap_old, ap_new, x, y) != 1:
		whoami.SayTo(activator, "\nSomething is very wrong... Call a DM!");
	else:
		pinfo.slaying = pid
		whoami.SayTo(activator, "\nDone! Your new apartment is ready.");

	pinfo.slaying = pid
	if pinfo != None:
		pinfo.last_heal = -1

if msg == 'hello' or msg == 'hi' or msg == 'hey':
	whoami.SayTo(activator, "\nWelcome to the apartment house.\nI can sell you an ^apartment^.\nI have ^cheap^, ^normal^, ^expensive^ and ^luxurious^ ones.\nSay ^invited^ to check if you have been invited by somebody to their apartment.");
elif msg == 'apartment':
	if pinfo != None:
		pinfo.last_heal = -1
	whoami.SayTo(activator, "\nAn apartment is a kind of unique place you can buy.\nOnly you can enter it!\nYou can safely store or drop your items there.\nThey will not vanish with the time.\nIf you leave the game they will be still there when you come back later.\nApartments have different sizes and styles.\nYou can have only one apartment at once in a city,\nbut you can ^upgrade^ it.");
elif msg == 'upgrade':
	if pinfo == None:
		whoami.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		pinfo.last_heal = -1
 	whoami.SayTo(activator, "\nApartment upgrading will work like this:\n1.) Choose your new home in the upgrade procedure.\n2.) You get *no* money back for your old apartment.\n3.) All items in your old apartment are *automatically* transfered, including items in containers. They appear in a big pile in your new apartment.\n4.) Your old apartment is exchanged with your new one.\nUpgrading will also work to change an expensive apartment to a cheap one.\nGo to upgrade ^procedure^ if you want to upgrade now.");
elif msg == 'procedure':
	if pinfo == None:
		whoami.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		pinfo.last_heal = -1
	if (pinfo.slaying == appid_cheap):
		whoami.SayTo(activator, "\nYour current home here is a cheap apartment.\nYou can upgrade it to normal, expensive or luxurious.\nTo upgrade say ^upgrade to normal apartment^,\n^upgrade to expensive apartment^ or\n^upgrade to luxurious apartment^.")
	elif (pinfo.slaying == appid_normal):
		whoami.SayTo(activator, "\nYour current home here is a normal apartment.\nYou can upgrade it to cheap, expensive or luxurious.\nTo upgrade say ^upgrade to cheap apartment^\n^upgrade to expensive apartment^ or\n^upgrade to luxurious apartment^.")
	elif (pinfo.slaying == appid_expensive):
		whoami.SayTo(activator, "\nYour current home here is a expensive apartment.\nYou can upgrade it to cheap, normal or luxurious.\nTo upgrade say ^upgrade to cheap apartment^\n^upgrade to normal apartment^ or\n^upgrade to luxurious apartment^.")
	else:
		whoami.SayTo(activator, "\nYour current home here is a luxurious apartment.\nYou can upgrade it to cheap, normal or expensive.\nTo upgrade say ^upgrade to cheap apartment^\n^upgrade to normal apartment^ or\n^upgrade to expensive apartment^.")
	whoami.SayTo(activator, "After you have said the sentence I will ask you to confirm.", 1)
elif msg == 'upgrade to cheap apartment':
	if pinfo == None:
		whoami.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		if (pinfo.slaying == appid_cheap):
			pinfo.last_heal = -1
			whoami.SayTo(activator, "\nYou can't upgrade to the same size.")
		else:
			pinfo.last_heal = 1
			whoami.SayTo(activator, "\nThe cheap apartment will cost you 30 silver.\nNOW LISTEN: To do the upgrade you must confirm it.\nDo you really want to upgrade to a cheap apartment now?\nSay YES, I DO and it will be done.\nSay anything else to cancel it.")
elif msg == 'upgrade to normal apartment':
	if pinfo == None:
		whoami.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		if (pinfo.slaying == appid_normal):
			pinfo.last_heal = -1
			whoami.SayTo(activator, "\nYou can't upgrade to the same size.")
		else:
			pinfo.last_heal = 2
			whoami.SayTo(activator, "\nThe normal apartment will cost you 250 silver.\nNOW LISTEN: To do the upgrade you must confirm it.\nDo you really want to upgrade to a normal apartment now?\nSay YES, I DO and it will be done.\nSay anything else to cancel it.")
elif msg == 'upgrade to expensive apartment':
	if pinfo == None:
		whoami.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		if (pinfo.slaying == appid_expensive):
			pinfo.last_heal = -1
			whoami.SayTo(activator, "\nYou can't upgrade to the same size.")
		else:
			pinfo.last_heal = 3
			whoami.SayTo(activator, "\nThe expensive apartment will cost you 15 gold.\nNOW LISTEN: To do the upgrade you must confirm it.\nDo you really want to upgrade to an expensive apartment now?\nSay YES, I DO and it will be done.\nSay anything else to cancel it.")
elif msg == 'upgrade to luxurious apartment':
	if pinfo == None:
		whoami.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		if (pinfo.slaying == appid_luxurious):
			pinfo.last_heal = -1
			whoami.SayTo(activator, "\nYou can't upgrade to the same size.")
		else:
			pinfo.last_heal = 4
			whoami.SayTo(activator, "\nThe luxurious apartment will cost you 200 gold.\nNOW LISTEN: To do the upgrade you must confirm it.\nDo you really want to upgrade to a luxurious apartment now?\nSay YES, I DO and it will be done.\nSay anything else to cancel it.")
elif msg == 'yes, i do':
	if pinfo == None:
		whoami.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		if pinfo.last_heal <= 0:
			whoami.SayTo(activator, "\nGo first to the upgrade ^procedure^ before you confirm your choice.")
		else:
			if pinfo.slaying == appid_cheap:
				old_ap = ap_1
			elif pinfo.slaying == appid_normal:
				old_ap = ap_2
			elif pinfo.slaying == appid_expensive:
				old_ap = ap_3
			elif pinfo.slaying == appid_luxurious:
				old_ap = ap_4

			if pinfo.last_heal == 1:
				if activator.PayAmount(3000) != 1:
					whoami.SayTo(activator, "\nSorry, you don't have enough money.");
				else:
					update_ap(old_ap, ap_1, appid_cheap, 1, 2)
			elif pinfo.last_heal == 2:
				if activator.PayAmount(25000) != 1:
					whoami.SayTo(activator, "\nSorry, you don't have enough money.");
				else:
					update_ap(old_ap, ap_2, appid_normal, 1, 2)
			elif pinfo.last_heal == 3:
				if activator.PayAmount(150000) != 1:
					whoami.SayTo(activator, "\nSorry, you don't have enough money.");
				else:
					update_ap(old_ap, ap_3, appid_expensive, 1, 2)
			elif pinfo.last_heal == 4:
				if activator.PayAmount(2000000) != 1:
					whoami.SayTo(activator, "\nSorry, you don't have enough money.");
				else:
					update_ap(old_ap, ap_4, appid_luxurious, 2, 1)
		pinfo.last_heal = -1
elif msg == 'pocket' or msg == 'dimension' or msg == 'pocket dimension':
	if pinfo != None:
		pinfo.last_heal = -1
	whoami.SayTo(activator, "\nA pocket dimension is a magical mini dimension\nin an outer plane. They are very safe and no thief will ever be able to enter.");
elif msg == 'cheap':
	if pinfo != None:
		pinfo.last_heal = -1
	whoami.SayTo(activator, "\nThe cheap apartment will cost you 30 silver.\nIt has only a bed and a chest.\nEvery apartment is a kind of ^pocket dimension^.\nYou can enter it by using the teleporter there.\nSay ^sell me a cheap apartment^ to buy it!\nChoose wisely!");
elif msg == 'normal':
	if pinfo != None:
		pinfo.last_heal = -1
	whoami.SayTo(activator, "\nThe normal apartment will cost you 250 silver.\nIt has some storing devices and some furniture.\nEvery apartment is a kind of ^pocket dimension^.\nYou can enter it by using the teleporter there.\nSay ^sell me a normal apartment^ to buy it!\nChoose wisely!");
elif msg == 'expensive':
	if pinfo != None:
		pinfo.last_heal = -1
	whoami.SayTo(activator, "\nThe expensive apartment will cost you 15 gold.\nIt is large for a single apartment and has many places to store items including a nice bed room.\nEvery apartment is a kind of ^pocket dimension^.\nYou can enter it by using the teleporter there.\nSay ^sell me an expensive apartment^ to buy it!\nChoose wisely!");
elif msg == 'luxurious':
	if pinfo != None:
		pinfo.last_heal = -1
	whoami.SayTo(activator, "\nThe luxurious apartment will cost you 200 gold.\nIt is very large for a single apartment and has a lot of places to store items including a nice bed room.\nEvery apartment is a kind of ^pocket dimension^.\nYou can enter it by using the teleporter there.\nSay ^sell me a luxurious apartment^ to buy it!\nChoose wisely!");
elif msg == 'sell me a cheap apartment':
	if pinfo == None:
		if activator.PayAmount(3000) == 1:
			activator.Write("You pay the money.", 0)
			pinfo = activator.CreatePlayerInfo(sglow_app_tag)
			pinfo.slaying = appid_cheap
			activator.Write("Darlin is casting some strange magic.", 0)
			whoami.SayTo(activator, "\nCongratulations! That was all!\nI have summoned your apartment right now.\nEnter the teleporter and you will be there!\nHave a nice day.");
		else:
			whoami.SayTo(activator, "\nSorry, you don't have enough money.");
	else:
		pinfo.last_heal = -1
		whoami.SayTo(activator, "\nYou have bought an apartment in the past here.\nYou can ^upgrade^ it.");
elif msg == 'sell me a normal apartment':
	if pinfo == None:
		if activator.PayAmount(25000) == 1:
			activator.Write("You pay the money.", 0)
			pinfo = activator.CreatePlayerInfo(sglow_app_tag)
			pinfo.slaying = appid_normal
			activator.Write("Darlin is casting some strange magic.", 0)
			whoami.SayTo(activator, "\nCongratulations! That was all!\nI have summoned your apartment right now.\nEnter the teleporter and you will be there!\nHave a nice day.");
		else:
			whoami.SayTo(activator, "\nSorry, you don't have enough money.");
	else:
		pinfo.last_heal = -1
		whoami.SayTo(activator, "\nYou have bought an apartment in the past here.\nYou can ^upgrade^ it.");
elif msg == 'sell me an expensive apartment':
	if pinfo == None:
		if activator.PayAmount(150000) == 1:
			activator.Write("You pay the money.", 0)
			pinfo = activator.CreatePlayerInfo(sglow_app_tag)
			pinfo.slaying = appid_expensive
			activator.Write("Darlin is casting some strange magic.", 0)
			whoami.SayTo(activator, "\nCongratulations! That was all!\nI have summoned your apartment right now.\nEnter the teleporter and you will be there!\nHave a nice day.");
		else:
			whoami.SayTo(activator, "\nSorry, you don't have enough money.");
	else:
		pinfo.last_heal = -1
		whoami.SayTo(activator, "\nYou already own an apartment here.\nYou can ^upgrade^ it.");
elif msg == 'sell me a luxurious apartment':
	if pinfo == None:
		if activator.PayAmount(2000000) == 1:
			activator.Write("You pay the money.", 0)
			pinfo = activator.CreatePlayerInfo(sglow_app_tag)
			pinfo.slaying = appid_luxurious
			activator.Write("Darlin is casting some strange magic.", 0)
			whoami.SayTo(activator, "\nCongratulations! That was all!\nI have summoned your apartment right now.\nEnter the teleporter and you will be there!\nHave a nice day.");
		else:
			whoami.SayTo(activator, "\nSorry, you don't have enough money.");
	else:
		pinfo.last_heal = -1
		whoami.SayTo(activator, "\nYou already own an apartment here.\nYou can ^upgrade^ it.");
elif msg == 'invited':
	apartment_force = activator.CheckInventory(0, "force", "APARTMENT_INVITE")
	if (apartment_force == None):
		whoami.SayTo(activator, "\nYou have not been invited by anybody.")
	else:
		inviter = FindPlayer(apartment_force.race)
		if inviter == None:
			whoami.SayTo(activator, "\nYou have been invited by %s.\nUnfortunately, this player is not in game anymore.\nYour invite has expired." % apartment_force.race)
			apartment_force.Remove()
		elif inviter.map.path != apartment_force.slaying:
			whoami.SayTo(activator, "\nYou have been invited by %s.\nIt seems like this player left his apartment.\nYour invite has expired." % apartment_force.race)
			apartment_force.Remove();
		else:
			whoami.SayTo(activator, "\nYou have been invited by %s.\nSay ^join apartment^ now to join %s's apartment!" % (apartment_force.race, apartment_force.race))
elif msg == 'join apartment':
	apartment_force = activator.CheckInventory(0, "force", "APARTMENT_INVITE")
	if (apartment_force == None):
		whoami.SayTo(activator, "\nYou have not been invited by anybody.")
	else:
		inviter = FindPlayer(apartment_force.race)
		if inviter == None:
			whoami.SayTo(activator, "\nYou have been invited by %s.\nUnfortunately, this player is not in game anymore.\nYour invite has expired." % apartment_force.race)
			apartment_force.Remove()
		elif inviter.map.path != apartment_force.slaying:
			whoami.SayTo(activator, "\nYou have been invited by %s.\nIt seems like this player left his apartment.\nYour invite has expired." % apartment_force.race)
			apartment_force.Remove();
		else:
			activator.TeleportTo(apartment_force.slaying, apartment_force.hitpoints, apartment_force.spellpoints, 0)
			apartment_force.Remove()
else:
	if pinfo != None and pinfo.last_heal > 0:
		whoami.SayTo(activator, "\nUpgrade canceled!\nYou have not said YES, I DO!");
		pinfo.last_heal = -1
