from Atrinik import *
import json
import random

def get_waypoints():
    for obj in activator.inv:
        if obj != me and obj.type == Type.WAYPOINT_OBJECT:
            yield obj

def main():
    event = WhatIsEvent()

    if event.msg:
        opts = json.loads(event.msg)
    else:
        opts = {}

    nextwp = random.choice(list(get_waypoints()))

    opt_pause = opts.get("pause")

    if opt_pause:
        nextwp.wc = random.randint(*opt_pause)

    me.title = nextwp.name

main()
