import Atrinik

# Just a basic test of waypoint triggers

monster  = Atrinik.WhoIsActivator() # The monster
waypoint = Atrinik.WhoAmI()         # The waypoint

# Do something
monster.Say('I just reached the waypoint "' + waypoint.name + '"')
