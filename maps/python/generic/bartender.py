## @file
## Generic script to handle bartenders, using the Tavern API.

from Atrinik import *
from Tavern import handle_bartender

# As this is a generic script, simply call the handling function with the
# relevant parameters.
handle_bartender(WhoIsActivator(), WhoAmI(), WhatIsMessage().strip().lower(), WhatIsEvent())
