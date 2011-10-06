## @file
## Implements the /stime command.

import datetime

activator.Write("\nCurrent server time:\n{0}".format(datetime.datetime.now().strftime("%a %b %d %Y, %H:%M:%S")), COLOR_WHITE)
