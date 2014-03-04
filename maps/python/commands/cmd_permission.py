## @file
## Implements the /cmd_permission command.

import re
from Interface import Interface
from Markup import markup_escape

def main():
    match = re.match(r"((?:\")(.+)(?:\")|([^ ]+))( (removeall|(add|remove) ([^ ]+)|add))?", msg)

    if not match:
        activator.Controller().DrawInfo(
            markup_escape("Usage: /cmd_permission <[\"]player name[\"]>"),
            color = COLOR_WHITE
        )
        return

    player = match.group(2) or match.group(3)
    action = match.group(6) or match.group(5)
    cmd_permission = match.group(7)

    pl = FindPlayer(player)

    if not pl:
        activator.Controller().DrawInfo("No such player.", color = COLOR_RED)
        return

    if action == "add":
        if cmd_permission:
            if not cmd_permission in pl.Controller().cmd_permissions:
                pl.Controller().cmd_permissions.append(cmd_permission)
            else:
                inf.add_msg("{activator.name} already has that permission.", color = COLOR_RED)
        else:
            inf.set_text_input(prepend = "/cmd_permission {} add ".format(activator.name))
    elif action == "remove":
        try:
            pl.Controller().cmd_permissions.remove(cmd_permission)
        except ValueError:
            inf.add_msg("{activator.name} does not have that permission.", color = COLOR_RED)
    elif action == "removeall":
        pl.Controller().cmd_permissions.clear()

    inf.add_msg("{activator.name} has the following permissions:\n")

    for tmp in pl.Controller().cmd_permissions:
        if tmp:
            inf.add_msg("\n    {perm} [y=-2][a=:/cmd_permission {activator.name} remove {perm}]x[/a]", perm = tmp, newline = False)

    inf.add_link("Add permission", dest = "/cmd_permission {} add".format(activator.name))
    inf.add_link("Remove all permissions", dest = "/cmd_permission {} removeall".format(activator.name))

inf = Interface(activator, activator)
main()
inf.finish()
