## @file
## Helpful NPC to change background music of a map.

from Atrinik import *
import re, struct

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nWelcome. I am the music changer -- just tell me which background music file to play and I will change this map to play it. You can find a list of media files <a=url:http://www.atrinik.org/page/client_media_files>here</a>. Say <a>none</a> to stop any music on the map.")
	elif msg == "none" or re.match("([a-zA-Z0-9_\-]+)\.(\w+)[ 0-9\-]?", msg):
		me.map.bg_music = None if msg == "none" else WhatIsMessage().strip()

		for player in me.map.GetPlayers():
			map_music = me.map.bg_music if me.map.bg_music else "no_music"
			# Construct new map name from old one.
			map_name = "<b><o=0,0,0>{}</o></b>".format(re.sub(r"\([a-zA-Z0-9_\-]+\)", "({})".format(map_music), me.map.name))
			# Yes, this is a big hack, as there is no way other way to send
			# new map name to the client. Sue me.
			player.Controller().WriteToSocket(2, struct.pack("2b" + str(len(map_name) + 1) + "s" + str(len(map_music) + 1) + "s4b", 1, 1, map_name, map_music, 0, 0, player.x, player.y))

main()
