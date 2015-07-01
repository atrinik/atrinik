## @file
## Implements the /stime command.

import datetime

from Atrinik import *


pl.DrawInfo("\nCurrent server time:\n{0}".format(datetime.datetime.now().strftime("%a %b %d %Y, %H:%M:%S")), COLOR_WHITE)
