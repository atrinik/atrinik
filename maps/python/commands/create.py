## @file
## Implements the /create DM command.

import re
from Common import obj_assign_attribs

def main():
    msg = WhatIsMessage()

    if not msg:
        return

    match = re.match(r"(?:(\d+) )?([^ ]+)(?: of (\w+))?(?: (.+))?", msg)

    if not match:
        return

    (num, archname, artname, attribs) = match.groups()

    if not num:
        num = 1
    else:
        num = int(num)

    for i in range(num):
        try:
            obj = CreateObject(archname)
        except AtrinikError as err:
            pl.DrawInfo(str(err), COLOR_RED)
            break

        obj.f_identified = True

        if artname:
            try:
                obj.Artificate(artname)
            except AtrinikError as err:
                obj.Destroy()
                pl.DrawInfo(str(err), COLOR_RED)
                break

        if obj.type == Type.PLAYER:
            obj.type = Type.MONSTER
            obj.f_monster = True

        obj_assign_attribs(obj, attribs)

        if obj.f_monster:
            activator.map.Insert(obj, activator.x, activator.y)
            obj.Update()

            if obj.randomitems:
                obj.CreateTreasure(obj.randomitems, obj.level)
        else:
            obj.InsertInto(activator)

main()
