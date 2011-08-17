## @file
## Generic script to handle bartenders, using the Tavern API.

from Atrinik import *
from Tavern import Bartender

bartender = Bartender(WhoIsActivator(), WhoAmI(), WhatIsEvent())
bartender.handle_msg(WhatIsMessage().strip().lower())
