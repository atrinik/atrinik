## @file
## Implements the /roll command.

from Atrinik import *
from random import randrange

activator = WhoIsActivator()
params = WhatIsMessage()

# Limits.
MIN_num = 1
MAX_num = 10
MIN_sides = 1
MAX_sides = 1000

# Parse parameters.
def parse_params():
	# No params
	if not params:
		return None

	# Split it.
	l = params.split("d")

	# Do validation. Are params in '<x>d<y>' format, and are both x and y digits?
	if len(l) < 2 or not l[0].isdigit() or not l[1].isdigit():
		return None

	# Return how many times to roll the die, and how many sides it should have.
	return (max(MIN_num, min(MAX_num, int(l[0]))), max(MIN_sides, min(MAX_sides, int(l[1]))))

def main():
	parse = parse_params()

	if not parse:
		activator.Write("Usage: /roll <times>d<sides>", COLOR_RED)
		return

	(num, sides) = parse
	results = []

	# Roll the die so many times.
	for die in range(num):
		results.append(str(randrange(sides) + 1))

	# Tell everyone about the roll.
	activator.map.Message(activator.x, activator.y, MAP_INFO_NORMAL, "{0} rolls a magical die ({1}d{2}) and gets: {3}.".format(activator.name, num, sides, ", ".join(results)), COLOR_ORANGE)

main()
