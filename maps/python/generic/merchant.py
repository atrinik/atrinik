## @file
## Generic script to handle merchants.

from Atrinik import *
from Market import handle_merchant

handle_merchant(WhoIsActivator(), WhoAmI(), WhatIsMessage().strip().lower(), WhatIsEvent())
