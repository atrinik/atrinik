"""
Implements the /roll command.
"""

from Atrinik import *
from random import randrange
from Markup import markup_escape

# Limits.
MIN_num = 1
MAX_num = 10
MIN_sides = 1
MAX_sides = 1000


# Parse parameters.
def parse_params():
    """
    Parse command parameters, eg, '1d5'.
    :return: Tuple containing how many times to roll, and how many sides it
             should have.
    :rtype: tuple
    """

    try:
        if not msg:
            return None
    except NameError:
        return None

    # Split it.
    l = msg.split("d")

    # Do validation. Are params in '<x>d<y>' format, and are both x and y
    # digits?
    if len(l) < 2 or not l[0].isdigit() or not l[1].isdigit():
        return None

    # Return how many times to roll the die, and how many sides it should have.
    return (max(MIN_num, min(MAX_num, int(l[0]))),
            max(MIN_sides, min(MAX_sides, int(l[1]))))


def main():
    parse = parse_params()

    if parse is None:
        pl.DrawInfo(markup_escape("Usage: /roll <times>d<sides>"),
                    color=COLOR_RED)
        return

    num, sides = parse

    # Tell everyone about the roll.
    message = "{} rolls a magical die ({}d{}) and gets: {}.".format(
        activator.name, num, sides,
        ", ".join(str(randrange(sides) + 1) for _ in range(num))
    )
    activator.map.DrawInfo(activator.x, activator.y, message, COLOR_ORANGE)

main()
