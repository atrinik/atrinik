## @file
## Implements the /cmd_permission command.

import re
from Interface import Interface
from Markup import markup_escape, markup_unescape

def main():
    match = re.match(r"((?:\")(.+)(?:\")|([^ ]+))"
            "( (removeall|(add|remove) ([^ ]+)|add))?", msg)

    if not match:
        activator.Controller().DrawInfo(
                "Usage: /cmd_permission <[]\"]player name[]\"]>",
                color = COLOR_WHITE)
        return

    player = match.group(2) or match.group(3)
    action = match.group(6) or match.group(5)
    cmd_permission = match.group(7)

    pl = FindPlayer(player)

    if not pl:
        activator.Controller().DrawInfo("No such player.", color = COLOR_RED)
        return

    inf = Interface(activator, pl)

    if cmd_permission:
        cmd_permission = markup_unescape(cmd_permission)

    if action == "add":
        if cmd_permission:
            if not cmd_permission in pl.Controller().cmd_permissions:
                pl.Controller().cmd_permissions.append(cmd_permission)
            else:
                inf.add_msg("{pl.name} already has that permission.",
                        color = COLOR_RED, pl = pl)
        else:
            inf.set_text_input(prepend = "/cmd_permission {} add ".format(
                    pl.name))
    elif action == "remove":
        try:
            pl.Controller().cmd_permissions.remove(cmd_permission)
        except ValueError:
            inf.add_msg("{pl.name} does not have that permission.",
                    color = COLOR_RED, pl = pl)
    elif action == "removeall":
        pl.Controller().cmd_permissions.clear()

    inf.add_msg("{pl.name} has the following permissions:\n", pl = pl)

    for tmp in pl.Controller().cmd_permissions:
        if tmp:
            inf.add_msg("\n    {perm} [y=-2][a=:/cmd_permission "
                    "{pl.name} remove {perm}]x[/a]",
                    perm = markup_escape(tmp), newline = False, pl = pl)

    inf.add_link("Add permission",
            dest = "/cmd_permission {} add".format(pl.name))
    inf.add_link("Remove all permissions",
            dest = "/cmd_permission {} removeall".format(pl.name))
    inf.finish()

main()
