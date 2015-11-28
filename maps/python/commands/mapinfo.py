## @file
## Implements the /mapinfo command.

from Atrinik import *

def main():
    for msg in (
        "{map.name} ({map.bg_music}, {map.path}, x: {activator.x}, y: "
        "{activator.y})".format(map = activator.map, activator = activator),
        "Players: {num_players} difficulty: {map.difficulty} size: "
        "{map.width}x{map.height} start: {map.enter_x}x{map.enter_y}".format(
            num_players = activator.map.CountPlayers(), map = activator.map,
        ),
        activator.map.msg,
               ):
        if msg:
            activator.Controller().DrawInfo(msg, color = COLOR_WHITE)

main()
