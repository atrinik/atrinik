from Interface import Interface

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
inf = Interface(activator, me)

def main():
	if msg == "hello":
		import random

		replies = [
			"Get away from my *hic* beer *hic*!",
			"I can see you! *hic* Don' make me come af'er you! *hic* ... Can ye find my sword? I think I losst it!",
			"Bartender, 'nother beer here!",
			"<yellow>The " + me.name + " breaks into a song...</yellow>\n\nI am a guard aaaand I am havin' a beer. Drinky drinky guard, drinky drinky guard.",
			"*HIC*",
		]

		inf.add_msg(random.choice(replies))

main()
inf.finish()
