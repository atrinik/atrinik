## @file
## Script to make a monster shout its message when it dies.

def main():
	me.map.Message(me.x, me.y, MAP_INFO_NORMAL, WhatIsEvent().msg.format(me.name), COLOR_NAVY)

main()
