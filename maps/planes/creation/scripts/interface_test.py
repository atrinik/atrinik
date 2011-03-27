## @file
## Experimental NPC interface.

from Atrinik import *

activator = WhoIsActivator()
pl = activator.Controller()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if pl.target_object != me:
		me.SayTo(activator, "\nTarget me and press the Hello button")
		return

	if msg == "hi" or msg == "hey" or msg == "hello":
		pl.WriteToSocket(30, "X<book>{}</book><img={} -10 -15>\n\nThis is NPC interface test.\n\nOur offers:\n\n<size=14><c=#000000><u><a=:/t_tell buy 10></o>Buy 10 beer</a></c></u></size>\n<size=14><c=#000000>Buy 10 cakes</c></size><y=2> -- currently out of stock<y=-2>".format(me.name, "monk.131"))
	elif msg == "buy 10":
		pl.WriteToSocket(30, "X<book>{}</book><img={} -10 -15>\n\nBeer's bad for you!\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nJust kidding!\n\n<img=beer.101>\n\n\n\n\n\n\n ".format(me.name, "monk.131"))

main()
