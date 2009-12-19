## @file
## Generic script for post office mailboxes.

from Atrinik import *
from PostOffice import *

## Activator object.
activator = WhoIsActivator()

post = PostOffice(activator.name)

# Have they got any accepted item?
if post.has_accepted_item():
	removed = 0

	for i, item in enumerate(post.get_items()):
		if item["accepted"] == 1:
			LoadObject(item["contents"]).InsertInside(activator)
			activator.Write("You receive '%s' from %s." % (item["name"], item["from"]))
			post.remove_item(i - removed)
			removed += 1

# No accepted items but they have some in the post office?
elif post.get_items():
	activator.Write("You have some items for you in the post office, but you have not yet accepted them.")

# Nope, no items...
else:
	activator.Write("There are no items for you.")

post.db.close()
# So we can't actually open/close the mailbox.
SetReturnValue(1)
