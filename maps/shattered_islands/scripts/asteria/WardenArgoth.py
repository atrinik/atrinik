## @file
## Implements the angry drunk Warden who drops Asteria Dungeon Key 1.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if me.enemy:
		if me.enemy == activator:
			me.SayTo(activator, "\nNow you die!")
        elif msg == "drunk" or msg == "drunkard"
            me.SayTo(activator, "\nKeep it up and you're next, buddy!")
		else:
			me.SayTo(activator, "\nCan't you see I'm busy here?!")

		return

	if msg == "hello" or msg == "hi" or msg == "hey":
		me.Communicate("/yawn")
		me.SayTo(activator, "\nHello {0}, I am {1}.  So what ^brings^ you here?".format(activator.name, me.name))

	elif msg == "brings":
		me.SayTo(activator, "\nI don't suppose you've come about the ^key^.  Sorry, I can't give that to you.")

	elif msg == "key":
		me.SayTo(activator, "\nI told you, I can't give that to you.")

	elif msg == "drunk" or msg == "drunkard":
		try:
			num = int(me.ReadKey("drunk_called"))
		except:
			num = 0

		if num == 0:
			me.SayTo(activator, "\nWhat did you call me?")
		elif num == 1:
			me.SayTo(activator, "\nDon't call me that, I hate it!")
		elif num == 8:
			me.SayTo(activator, "\nThat's it! The next one who says I'm a drunk dies!")
		elif num == 9:
			me.SayTo(activator, "\nNow you've done it! You'll pay for that!")
			me.enemy = activator
			me.WriteKey("drunk_called")
			return
		else:
			me.SayTo(activator, "\nI told you, I hate that!")

		me.WriteKey("drunk_called", str(num + 1))

main()
