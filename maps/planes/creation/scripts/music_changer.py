## @file
## Helpful NPC to change background music of a map.

from Atrinik import *
import re

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome. I am the music changer -- just tell me which background music file to play and I will change this map to play it. You can find a list of media files <a=url:http://www.atrinik.org/page/client_media_files>here</a>. Say <a>none</a> to stop any music on the map.")
	elif msg == "none" or re.match("^([a-zA-Z0-9_\-]+)\.(\w+)[ 0-9\-]?$", msg):
		beacon = LocateBeacon("creation_music_changer")

		if not beacon or not beacon.env:
			return

		info = beacon.env
		info.slaying = "no_music" if msg == "none" else WhatIsMessage().strip()
		# Construct new map name from old one.
		info.race = re.sub(r"\([a-zA-Z0-9_\-\.]+\)", "({})".format(info.slaying), info.race)
		map_name = "<b><o=0,0,0>{}</o></b>".format(info.race)

		for player in me.map.GetPlayers():
			if player.x >= info.x and player.x <= info.x + info.hp and player.y >= info.y and player.y <= info.y + info.sp:
				player.Controller().SendPacket(17, "BsBs", 1, map_name, 2, info.slaying)

main()
