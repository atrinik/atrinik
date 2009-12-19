## @file
## Script for elf Maplevale in Greyton church.

from Atrinik import *

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
is_tabernacle = activator.GetGod() == "Tabernacle"

if msg == "hi" or msg == "hey" or msg == "hello" or not is_tabernacle:
	me.SayTo(activator, "\nGreetings, %s." % activator.name)

	if not is_tabernacle:
		me.SayTo(activator, "Oh, nevermind. I thought you were a follower of Tabernacle.\nI only talk with those who worship Tabernacle.", 1)
	else:
		me.SayTo(activator, "I see that you are a follower of Tabernacle. It is a great pleasure to meet you here. I am %s, a priest of the Elven god Llwyfen. I am here to learn the ^teachings^ of the god of Tabernacle." % me.name, 1)

elif msg == "teachings":
	me.SayTo(activator, "\nYes, words of wisdom. But alas, so far I have not been able to hear any particular teaching from the priests of Tabernacle. There seems to be none who ever had a direct ^interaction^ with the god.")

elif msg == "interaction":
	me.SayTo(activator, "\nYes, like seeing, listening, and talking to the god. Strange enough, none of the priest even knows the real name of the god of Tabernacle, and still call the god by the name of the altar! It seems your god has not ^revealed^ himself to anyone yet.")

elif msg == "revealed":
	me.SayTo(activator, "\nIn our legend, the Tabernacle was crafted by Vashla, the Goddess of Vanity. Some elves believe that this goddess is the God of Tabernacle but I have ^confirmed^ that these two are totally different.")

elif msg == "confirmed":
	me.SayTo(activator, "\nOh certainly! Vashla was the Goddess of Vanity. What do you expect? It was very difficult to satisfy her appetite for praise. That was one of the reasons the elves of Eromir refused to worship her. I know it because I once saw the book of liturgy.\n\nOh I shouldn't make fun of a goddess. Anyway, the point is, deities do not change and Vashla wouldn't have allowed her followers to worship her like you do. As I observe, you humans do not even have any other ritual than touching the altar, while fully enjoying his ^gifts^.")

elif msg == "gifts":
	me.SayTo(activator, "\nResurrections, of course! A great and powerful god he must be.\n\nHmmm... it leads me to wonder. If the god of Tabernacle is not Vashla, who is he then? What is his purpose and why is he helping you? Maybe human gods are different from Elven gods? Oh don't worry, I was merely thinking out loud.")
