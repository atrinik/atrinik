## @file
## Generic script for post office mailboxes.

from Atrinik import *
from PostOffice import *

activator = WhoIsActivator()
pl = activator.Controller()

post = PostOffice(activator.name)

def main():
	# Have they got any accepted item?
	if post.has_accepted_item():
		removed = 0

		for i, item in enumerate(post.get_items()):
			if item["accepted"] == 1:
				tmp = LoadObject(item["contents"])

				if not pl.CanCarry(tmp):
					activator.Write("The '{0}' from {1} is too heavy for you to carry.".format(item["name"], item["from"]), COLOR_RED)
					continue

				tmp.InsertInside(activator)
				activator.Write("You receive '{0}' from {1}.".format(item["name"], item["from"]))
				post.remove_item(i - removed)
				removed += 1

	# No accepted items but they have some in the post office?
	elif post.get_items():
		activator.Write("You have some items for you in the post office, but you have not yet accepted them.")

	# Nope, no items...
	else:
		activator.Write("There are no items for you.")

try:
	main()
finally:
	post.db.close()
	SetReturnValue(1)
