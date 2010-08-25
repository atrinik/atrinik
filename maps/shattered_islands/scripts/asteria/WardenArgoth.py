## @file
## Implements the angry drunk Warden who drops Asteria Dungeon Key 1.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
        me.Communicate("/yawn " + activator.name)
		me.SayTo(activator, "\nHello {0}, I am {1}.  So what ^brings^ you here?".format(activator.name, me.name))

	elif msg == "brings":
		me.SayTo(activator, "\nI don't suppose you've come about the ^key^.  Sorry, I can't give that to you.")

    elif msg == "key":
		me.SayTo(activator, "\nI told you, I can't give that to you.")

    elif msg == "drunk" or msg == "drunkard":
		me.SayTo(activator, "\nWhat did you call me?")
        ## TODO: Make him get mad when you call him this... 10 tries before he goes hostile.