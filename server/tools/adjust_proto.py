# Script to adjust prototype declarations file after it's been created.

import sys

if len(sys.argv) < 2:
	sys.exit(0)

# Open the passed file and read all the lines.
f = open(sys.argv[1], "r")
lines = f.readlines()
f.close()

# Re-open it in writing mode, truncating it, and re-add the lines.
f = open(sys.argv[1], "w")

for line in lines:
	# Line has format specifiers, adjust it.
	if line.find(", ...);") != -1:
		commas = line.count(", ")
		line = line.replace(", ...);", ", ...) __attribute__((format(printf, {0}, {1})));".format(commas, commas + 1))

	f.write(line)

f.close()
