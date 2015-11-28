## @file
## Implements the /tpregion command.

from Atrinik import *

def main():
    region_name = WhatIsMessage()

    if not region_name:
        activator.Controller().DrawInfo("Usage: /tpregion <region>",
                                        color = COLOR_WHITE)
        return

    region = GetFirst("region")

    while region is not None:
        if region.name.startswith(region_name) and region.map_first:
            try:
                activator.TeleportTo(region.map_first)
            except AtrinikError as e:
                activator.Controller().DrawInfo(str(e), color = COLOR_RED)

            break

        region = region.next

main()
