## @file
## Script for cook in Portu Baserrian tavern.

from Atrinik import *

msg = WhatIsMessage().strip().lower()

if WhoIsActivator().name == "Birjina Neskamea" and msg == "that food cooked yet, bero?":
	WhoAmI().Communicate("Yep. Here you are, ready to go.")
