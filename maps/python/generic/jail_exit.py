## @file
## Jail manager portal, used for the /arrest command.

from Atrinik import *
from Jail import Jail

# Jail the player for 5 minutes.
Jail(WhoAmI()).jail(WhoIsActivator(), 300, False)
