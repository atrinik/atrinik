## @file
## Jail manager portal, used for the /arrest command.

from Atrinik import *
from Jail import Jail

# Jail the player for 15 minutes.
Jail(WhoAmI()).jail(WhoIsActivator(), 900, False)
