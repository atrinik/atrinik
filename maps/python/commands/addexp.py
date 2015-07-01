## @file
## Implements the /addexp command.

import re

from Atrinik import *
from Markup import markup_escape


def main():
    match = re.match(r"((?:\")(.+)(?:\")|([^ ]+)) ((?:\")?([^\d]+)(?:\")?|(\d+)) (-?\d+)( (?:level|lvl)(?:s)?)?", WhatIsMessage() or "")

    if not match:
        activator.Controller().DrawInfo(
            markup_escape("Usage: /addexp <[\"]player name[\"]> <skill number|skill name> <experience|levels> ['levels']\n") +
            "Examples:\n"
            "[b]/addexp Dummy slash weapons 125[/b] - adds 125 exp to slash weapons of 'Dummy'\n"
            "[b]/addexp Dummy wizardry spells 100 levels[/b] - adds 100 levels to wizardry spells of 'Dummy'\n"
            "[b]/addexp \"Some Player\" 1 50 levels[/b] - adds 50 levels to literacy of 'Some Player'",
            color = COLOR_WHITE
        )
        return

    player = match.group(2) or match.group(3)
    skill = match.group(5)
    exp = int(match.group(7))
    levels = match.group(8)

    if not skill:
        skill = int(match.group(6))

    pl = FindPlayer(player)

    if not pl:
        activator.Controller().DrawInfo("No such player.", color = COLOR_RED)
        return

    pl.Controller().AddExp(skill, exp, True, True if levels else False)

main()
