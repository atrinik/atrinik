## @file
## Implements bards.
##
## The event can have a comma-separated list of maps to change all at once
## in its config option.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## All the possible areas.
areas = {
	"greyton_house": [("desert_town.mid", "Desert Town"), ("harmony2.mid", "Harmony"), ("joyful_life.ogg", "Joyful Life"), ("town_music.ogg", "Town Music"), ("campfire_tales.mid", "Campfire Tales"), ("fuego.ogg", "Fuego"), ("solemn.mid", "Solemn"), (None, "None")],
	"everlink": [("harmony2.mid", "Harmony"), ("joyful_life.ogg", "Joyful Life"), ("town_music.ogg", "Town Music"), (None, "None")],
}

# Get the current working area.
try:
	area = areas[GetOptions()]
except:
	raise error("Invalid area: '{0}'".format(GetOptions()))

# Remove songs that 1.1 client doesn't have.
if activator.Controller().s_socket_version < 1038:
	areas[GetOptions()].remove(("fuego.ogg", "Fuego"))

## Adjust the background music of a single map, updating background
## music of all players on that map.
## @param m What map to update.
## @param music New map music.
## @todo Perhaps this should be a Python plugin function?
def adjust_map_music(m, music):
	m.bg_music = music

	for player in m.GetPlayers():
		# 1.1 client doesn't support this, unfortunately.
		if player.Controller().s_socket_version >= 1038:
			player.Sound(m.bg_music and m.bg_music or "no_music", CMD_SOUND_BACKGROUND, 0, 0, -1)

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		l = list(map(lambda l: "^" + l[1] + "^", area))
		me.SayTo(activator, "\nGreetings {0}, I am {1}.\nI can play the following songs for everyone in range:\n{2}.".format(activator.name, me.name, ", ".join(l[:-2] + [""]) + " and ".join(l[-2:])))

	else:
		# Find the song.
		l = list(filter(lambda l: l[1].lower() == msg, area))

		if not l:
			return

		l = l[0]

		# Handle 'None' specially.
		if not l[0]:
			if me.map.bg_music:
				if activator.Controller().s_socket_version < 1038:
					me.SayTo(activator, "\nOkay, I will stop playing the current song when you re-enter the map.")
				else:
					me.SayTo(activator, "\nOkay, I will stop playing the current song now.")
			else:
				me.SayTo(activator, "\nI'm not playing anything at the moment.")
		else:
			if activator.Controller().s_socket_version < 1038:
				me.SayTo(activator, "\nOkay, I will play ~{0}~ then... Hold on one moment!\n...\nThere. I will start the new song when you re-enter the map.".format(l[1]))
			else:
				me.SayTo(activator, "\nOkay, I will play ~{0}~ then... Hold on one moment... There!".format(l[1]))

		event_msg = WhatIsEvent().msg

		if event_msg:
			from os import path

			for map_name in event_msg.split(","):
				m = ReadyMap(path.join(path.dirname(me.map.path), map_name.strip()))

				if not m:
					raise ValueError("ERROR: Event's message included map '{0}', but no such map could be loaded.".format(map_name))

				adjust_map_music(m, l[0])
		else:
			adjust_map_music(me.map, l[0])

main()
