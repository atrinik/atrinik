## @file
## Implements the /roll command.

from random import randrange

# Limits.
MIN_num = 1
MAX_num = 10
MIN_sides = 1
MAX_sides = 1000

# Parse parameters.
def parse_params():
	try:
		if not msg:
			return None
	except NameError:
		return None

	# Split it.
	l = msg.split("d")

	# Do validation. Are params in '<x>d<y>' format, and are both x and y digits?
	if len(l) < 2 or not l[0].isdigit() or not l[1].isdigit():
		return None

	# Return how many times to roll the die, and how many sides it should have.
	return (max(MIN_num, min(MAX_num, int(l[0]))), max(MIN_sides, min(MAX_sides, int(l[1]))))

def main():
	parse = parse_params()

	if not parse:
		pl.DrawInfo("Usage: /roll <times>d<sides>", COLOR_RED)
		return

	(num, sides) = parse

	# Tell everyone about the roll.
	activator.map.DrawInfo(activator.x, activator.y, "{} rolls a magical die ({}d{}) and gets: {}.".format(activator.name, num, sides, ", ".join(str(randrange(sides) + 1) for die in range(num))), COLOR_ORANGE)

main()
