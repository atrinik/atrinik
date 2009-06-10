import Atrinik

# Test to trigger waypoints from scripts

activator = Atrinik.WhoIsActivator() 
whoami    = Atrinik.WhoAmI()         
msg       = Atrinik.WhatIsMessage().strip().lower()

if msg == 'go':
    tmp = whoami.CheckInventory(0, None, "waypoint1")
    if tmp == None:
        whoami.SayTo(activator, 'Oops... I seem to be lost.')
    else:
        tmp.f_cursed = 1 # this activates the waypoint
else:
    whoami.SayTo(activator, 'Tell me to ^go^ to activate my waypoint')
