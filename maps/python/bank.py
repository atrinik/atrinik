from Atrinik import * 
import string

activator=WhoIsActivator()
whoami=WhoAmI()

msg = WhatIsMessage().strip().lower()
text = string.split(msg)
pinfo_tag = "BANK_GENERAL"

if msg == 'bank' or msg == 'hello' or msg == 'hi' or msg == 'hey':
	whoami.SayTo(activator,"\nHello! I am %s, the banker.\n What can i do for you? You need ^info^ about this bank?" % whoami.name )
elif msg == 'info':
	whoami.SayTo(activator,"\nWith the keyword ^balance^ i will tell how much money\nyou have stored here on your account.\nStore money with ^deposit^ <# gold, # silver ...>.\nGet money with ^withdraw^ <# gold, # silver ...>.")
elif msg == 'balance':
	pinfo = activator.GetPlayerInfo(pinfo_tag)
	if pinfo == None or pinfo.value == 0:
		whoami.Say(" %s, you have no money stored." % activator.name)
	else:
		whoami.Say(" %s, your balance is %s." % (activator.name, activator.ShowCost(pinfo.value)))
elif text[0] == 'deposit':
	pinfo = activator.GetPlayerInfo(pinfo_tag)
	if pinfo == None:
		pinfo = activator.CreatePlayerInfo(pinfo_tag) 
	dpose = activator.Deposit(pinfo, msg)
	if dpose == 0:
		whoami.Say(" %s, you don't have that much money." % activator.name)
	elif dpose == 1:
		if pinfo.value != 0:
			whoami.Say(" %s, your new balance is %s." % (activator.name, activator.ShowCost(pinfo.value)))
	

elif text[0] == 'withdraw':
	pinfo = activator.GetPlayerInfo(pinfo_tag)
	if pinfo == None or pinfo.value == 0:
		whoami.Say(" %s, you have no money stored." % activator.name)
	else:
		wdraw = activator.Withdraw(pinfo, msg)
		if  wdraw == 0:
			whoami.Say(" %s, you don't have that much money." % activator.name)
		elif wdraw == 1:
			if pinfo.value == 0:
				whoami.Say(" %s, you removed all your money." % activator.name)
			else:
				whoami.Say(" %s, your new balance is %s." % (activator.name, activator.ShowCost(pinfo.value)))
else:
	activator.Write("%s, the banker seems not to notice you.\nYou should try ^hello^, ^hi^ or ^hey^..." % whoami.name, 0)
