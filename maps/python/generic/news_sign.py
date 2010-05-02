## @file
## Generic script for various news signs around the world.

from Atrinik import *
from News import *

## Activator object.
activator = WhoIsActivator()

location = GetOptions()

# Raise an error if there is no location
if not location:
	raise error("Bogus news sign: no event options to indicate location set.")

news = News(location)

def main():
	messages = news.get_messages()

	if messages:
		for message in messages:
			activator.Write("{0}: {1}".format(message["time"], message["message"]), COLOR_NAVY)
	# No news
	else:
		activator.Write("There are no news.", COLOR_NAVY)

try:
	main()
finally:
	news.db.close()
	SetReturnValue(1)
