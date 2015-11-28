## @file
## Script to make a monster shout its message when it dies.

from Atrinik import *


def main():
    me.map.DrawInfo(me.x, me.y, WhatIsEvent().msg.format(me.name), COLOR_NAVY)

main()
