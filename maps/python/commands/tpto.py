## @file
## Implements the /tpto command.

import re
from Markup import markup_escape

def main():
    match = re.match(r"([^ ]+)(?: (\d+))?(?: (\d+))?(?: (unique)(?:\:((?:\")(.+)(?:\")|([^ ]+)))?)?", WhatIsMessage() or "")

    if not match:
        activator.Controller().DrawInfo(
            markup_escape("Usage: /tpto <path> [x] [y] ['unique'[':'[\"]player name[\"]]]\n") +
            "Examples:\n"
            "[b]/tpto /shattered_islands/world_0110[/b] - teleports to world_0110, at default map enter x/y\n"
            "[b]/tpto world_0505[/b] - teleports to world_0505 map, relative to the map path you're at\n"
            "[b]/tpto /hall_of_dms 2 18[/b] - teleports to Hall of DMs, at x: 2, y: 18\n"
            "[b]/tpto /shattered_islands/strakewood_island/apartment_luxurious unique[/b] - enter your luxurious apartment\n"
            "[b]/tpto /shattered_islands/strakewood_island/apartment_cheap unique:\"Some Player\"[/b] - enter the cheap apartment of 'Some Player'\n",
            color = COLOR_WHITE
        )
        return

    path = match.group(1)
    x = match.group(2) or -1
    y = match.group(3) or -1
    unique = match.group(4)
    player = match.group(6) or match.group(7) or activator.name

    path = activator.map.GetPath(path, unique = True if unique else False, name = player)

    try:
        activator.TeleportTo(path, int(x), int(y))
    except Exception as e:
        activator.Controller().DrawInfo(str(e), color = COLOR_RED)

main()
