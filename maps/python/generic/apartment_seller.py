## @file
## Script for the apartment seller in Brynknot.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

exec(open(CreatePathname("/python/generic/apartments.py")).read())

## Name of the apartment we're dealing with.
apartment_id = GetOptions()

## The apartments we're dealing with.
apartments = apartments_info[apartment_id]["apartments"]

## Tag ID of this apartment.
apartment_tag = apartments_info[apartment_id]["tag"]

msg = WhatIsMessage().strip().lower()
text = msg.split()

## The apartment's info
pinfo = activator.GetPlayerInfo(apartment_tag)

## Function to upgrade an old apartment to a new one.
def upgrade_apartment(ap_old, ap_new, pid, x, y):
	activator.Write("You pay the money.", 0)
	activator.Write("{0} is casting some strange magic.".format(me.name), 0)

	if activator.SwapApartments(ap_old, ap_new, x, y) != 1:
		me.SayTo(activator, "\nSomething is very wrong... Call a DM!")
	else:
		pinfo.slaying = pid
		me.SayTo(activator, "\nDone! Your new apartment is ready.")

	pinfo.slaying = pid

	if pinfo != None:
		pinfo.last_heal = -1

## Comparing function for sort_apartments().
def apartment_compare(x):
	return apartments[x]["id"]

## Sort apartments by their ID from dictionary.
## @param d The dictionary.
## @return The sorted list of apartments.
def sort_apartments(d):
	return sorted(d.keys(), key=apartment_compare)

# Greeting
if text[0] == "hello" or text[0] == "hi" or text[0] == "hey":
	apartment_links = []

	for apartment in sort_apartments(apartments):
		apartment_links.append("^{0}^".format(apartment))

	me.SayTo(activator, "\nWelcome to the apartment house.\nI can sell you an ^apartment^.\nI have {0} ones.".format(", ".join(apartment_links)))

# Explain what an apartment is
elif text[0] == "apartment":
	if pinfo != None:
		pinfo.last_heal = -1

	me.SayTo(activator, "\nAn apartment is a kind of unique place you can buy.\nOnly you can enter it!\nYou can safely store or drop your items there.\nThey will not vanish with the time.\nIf you leave the game they will be still there when you come back later.\nApartments have different sizes and styles.\nYou can have only one apartment at once in a city,\nbut you can ^upgrade^ it.")

# Explain how upgrading works
elif msg == "upgrade":
	if pinfo == None:
		me.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		pinfo.last_heal = -1

		me.SayTo(activator, "\nApartment upgrading will work like this:\n1.) Choose your new home in the upgrade procedure.\n2.) You get |no| money back for your old apartment.\n3.) All items in your old apartment are |automatically| transferred, including items in containers. They appear in a big pile in your new apartment.\n4.) Your old apartment is exchanged with your new one.\nUpgrading will also work to change expensive apartment to cheap one, for example.\nGo to upgrade ^procedure^ if you want to upgrade now.")

# Upgrade procedure.
elif text[0] == "procedure":
	if pinfo == None:
		me.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		pinfo.last_heal = -1

		upgrades = []
		upgrade_links = []

		for apartment in sort_apartments(apartments):
			if apartment != pinfo.slaying:
				upgrades.append(apartment)
				upgrade_links.append("^upgrade to {0} apartment^".format(apartment))

		me.SayTo(activator, "\nYour current home here is {0} apartment.\nYou can upgrade it to one of {1}.\nTo upgrade say one of:\n{2}\nAfter you have said the sentence I will ask you to confirm.".format(pinfo.slaying, ", ".join(upgrades), "\n".join(upgrade_links)))

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

			me.SayTo(activator, "\nThe {0} apartment will cost you {1}.\n~Now listen~: To do the upgrade you must confirm it.\nDo you really want to upgrade to {0} apartment now?\nSay ~Yes, I do~ and it will be done.\nSay anything else to cancel it.".format(text[2], activator.ShowCost(apartments[text[2]]["price"])))

# The player said "Yes, I do", and we can upgrade the apartment now.
elif msg == "yes, i do":
	if pinfo == None:
		me.SayTo(activator, "\nYou don't have any apartment to upgrade.")
	else:
		if pinfo.last_heal <= 0:
			me.SayTo(activator, "\nFirst go to the upgrade ^procedure^ before you confirm your choice.")
		else:
			apartment_info = apartments[apartments_info[apartment_id]["apartment_ids"][pinfo.last_heal]]

			old_apartment = apartments[pinfo.slaying]["path"]

			if activator.PayAmount(apartment_info["price"]) == 1:
				upgrade_apartment(old_apartment, apartment_info["path"], apartments_info[apartment_id]["apartment_ids"][pinfo.last_heal], apartment_info["x"], apartment_info["y"])
			else:
				me.SayTo(activator, "\nSorry, you don't have enough money.")

		pinfo.last_heal = -1

# Explain what pocket dimension is
elif text[0] == "pocket" or text[0] == "dimension" or msg == "pocket dimension":
	if pinfo != None:
		pinfo.last_heal = -1

	me.SayTo(activator, "\nA pocket dimension is a magical mini dimension in an outer plane. They are very safe and no thief will ever be able to enter.")

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

			activator.Write("{0} is casting some strange magic.".format(me.name), 0)
			me.SayTo(activator, "\nCongratulations! I have summoned your apartment.\nEnter the teleporter and you will be there!\nHave a nice day.")
		else:
			me.SayTo(activator, "\nSorry, you don't have enough money.")
	else:
		pinfo.last_heal = -1

		me.SayTo(activator, "\nYou have bought an apartment here in the past.\nYou can ^upgrade^ it.")

# Otherwise if the player said anything else, and they were in the upgrade procedure, cancel it.
else:
	if pinfo != None and pinfo.last_heal > 0:
		me.SayTo(activator, "\nUpgrade canceled!\nYou did not say ~Yes, I do~.")
		pinfo.last_heal = -1
