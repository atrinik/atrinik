## @file
## Script to make a monster shout its when it dies.

from Atrinik import *

me = WhoAmI()

me.map.Message(me.x, me.y, MAP_INFO_NORMAL, WhatIsEvent().msg.format(me.name), COLOR_NAVY)
