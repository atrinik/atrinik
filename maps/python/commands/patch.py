## @file
## Implements the /patch DM command.

import re
from Common import obj_assign_attribs, find_obj

def main():
    msg = WhatIsMessage()

    if not msg:
        return

    match = re.match(r'(?:(?:#\s*(\d+))|(?:"([^"]+)")|([^ ]+))\s+(.+)', msg)

    if not match:
        return

    count = match.group(1)
    name = match.group(2) or match.group(3)
    attribs = match.group(4)

    if count:
        count = int(count)

    obj = FindPlayer(name) if name else None

    if not obj:
        obj = find_obj(activator, archname = name, name = name, count = count)

    if not obj:
        pl.DrawInfo("No such object found.", COLOR_RED)
        return

    obj_assign_attribs(obj, attribs)

main()
