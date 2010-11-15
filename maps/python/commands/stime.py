## @file
## Implements the /stime command.

from Atrinik import *
import datetime

WhoIsActivator().Write("\nCurrent server time:\n{0}".format(datetime.datetime.now().strftime("%a %b %d %Y, %H:%M:%S")), COLOR_WHITE)
