## @file
## Script to allow connecting any object to a connection event.
##
## Configuration option format for this script is:
##
## beacon_name;map_path;ret
##
## Where:
##  beacon_name (required) = name of the beacon that is inside a
##  handle/lever/switch/etc which will be applied by the script
##  map_path (optional) = absolute or relative path to map which will
##  be loaded (so beacon can be found there, if there is any, allowing
##  remote map connections)
##  ret (optional) = what to return; for example, return 1 will usually
##  stop any normal apply action.
##
## The event may hold a message, which will be printed to the activator.

event = WhatIsEvent()

def main():
    # Get the configuration.
    options = GetOptions().split(";")
    m = me.map

    if len(options) > 1 and options[1]:
        m = m.GetPath(options[1])

    # Set the return value, if any.
    if len(options) > 2:
        SetReturnValue(int(options[2]))

    # Try to find the beacon.
    beacon = m.LocateBeacon(options[0])

    if not beacon or not beacon.env:
        raise AtrinikError("Could not find beacon {} or beacon is not inside inventory.".format(options[0]))

    # Apply the switch the beacon is in.
    me.Apply(beacon.env, APPLY_TOGGLE | APPLY_NO_EVENT)

    # Send a message, if any.
    if event.msg and pl:
        pl.DrawInfo(event.msg, COLOR_WHITE)

main()
