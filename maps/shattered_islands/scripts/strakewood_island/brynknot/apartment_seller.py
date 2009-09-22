from Atrinik import * 
import string, os
from inspect import currentframe

activator = WhoIsActivator()
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/apartments.py")

# To transfer IDs of apartments to strings.
apartment_ids = {
	1: "cheap",
	2: "normal",
	3: "expensive",
	4: "luxurious",
}

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

# The apartment's info
pinfo = activator.GetPlayerInfo(apartment_tag)

# Function to upgrade an old apartment to a new one.
def upgrade_apartment(ap_old, ap_new, pid, x, y):
	activator.Write("You pay the money.", 0)
	activator.Write("Darlin is casting some strange magic.", 0)

	if activator.SwapApartments(ap_old, ap_new, x, y) != 1:
		me.SayTo(activator, "\nSomething is very wrong... Call a DM!");
	else:
		pinfo.slaying = pid
		me.SayTo(activator, "\nDone! Your new apartment is ready.");

	pinfo.slaying = pid

	if pinfo != None:
		pinfo.last_heal = -1

# Greeting
if text[0] == "hello" or text[0] == "hi" or text[0] == "hey":
	me.SayTo(activator, "\nWelcome to the apartment house.\nI can sell you an ^apartment^.\nI have ^cheap^, ^normal^, ^expensive^ and ^luxurious^ ones.\nSay ^invited^ to check if you have been invited by somebody to their apartment.");

# Explain what an apartment is
elif text[0] == "apartment":
	if pinfo != None:
		pinfo.last_heal = -1

	me.SayTo(activator, "\nAn apartment is a kind of unique place you can buy.\nOnly you can enter it!\nYou can safely store or drop your items there.\nThey will not vanish with the time.\nIf you leave the game they will be still there when you come back later.\nApartments have different sizes and styles.\nYou can have only one apartment at once in a city,\nbut you can ^upgrade^ it.");

# Explain how upgrading works
elif msg == "upgrade":
	if pinfo == None:
		me.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		pinfo.last_heal = -1

		me.SayTo(activator, "\nApartment upgrading will work like this:\n1.) Choose your new home in the upgrade procedure.\n2.) You get |no| money back for your old apartment.\n3.) All items in your old apartment are |automatically| transferred, including items in containers. They appear in a big pile in your new apartment.\n4.) Your old apartment is exchanged with your new one.\nUpgrading will also work to change expensive apartment to cheap one.\nGo to upgrade ^procedure^ if you want to upgrade now.");

# Upgrade procedure.
# TODO: This could be improved to make use of the apartments dictionary.
elif text[0] == "procedure":
	if pinfo == None:
		me.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		pinfo.last_heal = -1

		if pinfo.slaying == "cheap":
			me.SayTo(activator, "\nYour current home here is cheap apartment.\nYou can upgrade it to normal, expensive or luxurious.\nTo upgrade say ^upgrade to normal apartment^,\n^upgrade to expensive apartment^ or\n^upgrade to luxurious apartment^.")
		elif pinfo.slaying == "normal":
			me.SayTo(activator, "\nYour current home here is normal apartment.\nYou can upgrade it to cheap, expensive or luxurious.\nTo upgrade say ^upgrade to cheap apartment^\n^upgrade to expensive apartment^ or\n^upgrade to luxurious apartment^.")
		elif pinfo.slaying == "expensive":
			me.SayTo(activator, "\nYour current home here is expensive apartment.\nYou can upgrade it to cheap, normal or luxurious.\nTo upgrade say ^upgrade to cheap apartment^\n^upgrade to normal apartment^ or\n^upgrade to luxurious apartment^.")
		elif pinfo.slaying == "luxurious":
			me.SayTo(activator, "\nYour current home here is luxurious apartment.\nYou can upgrade it to cheap, normal or expensive.\nTo upgrade say ^upgrade to cheap apartment^\n^upgrade to normal apartment^ or\n^upgrade to expensive apartment^.")

		me.SayTo(activator, "\nAfter you have said the sentence I will ask you to confirm.", 1)

# Upgrade an apartment. The text is dynamic and will support anything in the apartments dictionary.
# Text to activate this might look like this: "upgrade to cheap apartment"
elif text[0] == "upgrade" and text[1] == "to" and text[2] in apartments and text[3] == "apartment":
	if pinfo == None:
		me.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		if pinfo.slaying == text[2]:
			pinfo.last_heal = -1

			me.SayTo(activator, "\nYou can't upgrade to the same size.")
		else:
			pinfo.last_heal = apartments[text[2]]["id"]

			me.SayTo(activator, "\nThe %s apartment will cost you %s.\n~Now listen~: To do the upgrade you must confirm it.\nDo you really want to upgrade to a cheap apartment now?\nSay ~Yes, I do~ and it will be done.\nSay anything else to cancel it." % (text[2], activator.ShowCost(apartments[text[2]]["price"])))

# The player said "Yes, I do", and we can upgrade the apartment now.
elif msg == "yes, i do":
	if pinfo == None:
		me.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		if pinfo.last_heal <= 0:
			me.SayTo(activator, "\nFirst go to the upgrade ^procedure^ before you confirm your choice.")
		else:
			apartment_info = apartments[apartment_ids[pinfo.last_heal]]

			old_apartment = apartments[pinfo.slaying]["path"]

			if activator.PayAmount(apartment_info["price"]) == 1:
				upgrade_apartment(old_apartment, apartment_info["path"], apartment_ids[pinfo.last_heal], apartment_info["x"], apartment_info["y"])
			else:
				me.SayTo(activator, "\nSorry, you don't have enough money.");

		pinfo.last_heal = -1

# Explain what pocket dimension is
elif text[0] == "pocket" or text[0] == "dimension" or msg == "pocket dimension":
	if pinfo != None:
		pinfo.last_heal = -1

	me.SayTo(activator, "\nA pocket dimension is a magical mini dimension in an outer plane. They are very safe and no thief will ever be able to enter.");

# If the player asked about an apartment in the apartments dictionary, give out some information
elif text[0] in apartments:
	if pinfo != None:
		pinfo.last_heal = -1

	me.SayTo(activator, apartments[text[0]]["info"])

# Sell an apartment. The text might look like this: "sell me normal apartment"
elif text[0] == "sell" and text[1] == "me" and text[2] in apartments and text[3] == "apartment":
	if pinfo == None:
		apartment_info = apartments[text[2]]

		if activator.PayAmount(apartment_info["price"]) == 1:
			activator.Write("You pay the money.", 0)
			pinfo = activator.CreatePlayerInfo(apartment_tag)
			pinfo.slaying = text[2]

			activator.Write("%s is casting some strange magic." % me.name, 0)
			me.SayTo(activator, "\nCongratulations! I have summoned your apartment.\nEnter the teleporter and you will be there!\nHave a nice day.");
		else:
			me.SayTo(activator, "\nSorry, you don't have enough money.");
	else:
		pinfo.last_heal = -1

		me.SayTo(activator, "\nYou have bought an apartment here in the past.\nYou can ^upgrade^ it.");

# Check if the player has been invited by somebody
elif msg == "invited":
	apartment_force = activator.CheckInventory(0, "force", "APARTMENT_INVITE")

	if (apartment_force == None):
		me.SayTo(activator, "\nYou have not been invited by anybody.")
	else:
		inviter = FindPlayer(apartment_force.race)

		if inviter == None:
			me.SayTo(activator, "\nYou have been invited by %s.\nUnfortunately, this player is not in game anymore.\nYour invite has expired." % apartment_force.race)
			apartment_force.Remove()
		elif inviter.map.path != apartment_force.slaying:
			me.SayTo(activator, "\nYou have been invited by %s.\nIt seems like this player left his apartment.\nYour invite has expired." % apartment_force.race)
			apartment_force.Remove();
		else:
			me.SayTo(activator, "\nYou have been invited by %s.\nSay ^join apartment^ now to join %s's apartment!" % (apartment_force.race, apartment_force.race))

# Join the apartment now
elif msg == "join apartment":
	apartment_force = activator.CheckInventory(0, "force", "APARTMENT_INVITE")

	if (apartment_force == None):
		me.SayTo(activator, "\nYou have not been invited by anybody.")
	else:
		inviter = FindPlayer(apartment_force.race)

		if inviter == None:
			me.SayTo(activator, "\nYou have been invited by %s.\nUnfortunately, this player is not in game anymore.\nYour invite has expired." % apartment_force.race)
		elif inviter.map.path != apartment_force.slaying:
			me.SayTo(activator, "\nYou have been invited by %s.\nIt seems like this player left his apartment.\nYour invite has expired." % apartment_force.race)
		else:
			activator.TeleportTo(apartment_force.slaying, apartment_force.hitpoints, apartment_force.spellpoints, 0)

		apartment_force.Remove()

# Otherwise if the player said anything else, and they were in the upgrade procedure, cancel it.
else:
	if pinfo != None and pinfo.last_heal > 0:
		me.SayTo(activator, "\nUpgrade canceled!\nYou did not say ~Yes, I do~.");
		pinfo.last_heal = -1
