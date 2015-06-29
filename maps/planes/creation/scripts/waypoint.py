## @file
## Test of waypoint functionality.

from Atrinik import *

def main():
    # By checking event number we can handle multiple event types
    # in a single script.
    event_nr = GetEventNumber()

    # Handle say event.
    if event_nr == EVENT_SAY:
        from Interface import Interface

        inf = Interface(activator, me)

        # Greeting; inform them about the 'go' command.
        if msg == "hello":
            inf.add_msg("Do you want me to activate my waypoint?")
            inf.add_link("Yes.", dest = "yes")
        elif msg == "yes":
            # Activate the starting waypoint.
            me.FindObject(name = "waypoint1").f_cursed = True
            inf.dialog_close()

        inf.finish(disable_timeout = True)
    # Trigger event is triggered when the waypoint has reached its destination.
    elif event_nr == EVENT_TRIGGER:
        # Reached waypoint that is used for getting to apples dropped around a tree.
        if me.name == "apple waypoint":
            # Try to find the apple by looking at objects below the guard's feet.
            for obj in activator.map.GetFirstObject(activator.x, activator.y):
                # Is the object an apple?
                if obj.type == Type.FOOD and obj.arch.name == "apple":
                    # Remove the apple, inform the map and return.
                    obj.Destroy()
                    activator.Say("Omnomnomnomnom!")
                    return

            # The return wasn't used in the above loop, this means we could not find an
            # apple where we previously saw it...
            activator.Say("Hmpf! Somebody stole my apple...")
        # Inform the map about us reaching this waypoint.
        else:
            activator.Say("I just reached the waypoint [yellow]{}[/yellow].".format(me.name))
    # Close event is triggered whenever a waypoint makes a monster move.
    elif event_nr == EVENT_CLOSE:
        # Check for existing apple waypoint.
        apple_wp = activator.FindObject(name = "apple waypoint")

        if apple_wp:
            # There is an existing apple waypoint for this guard, so do not create
            # another one, but wait until it is inactive.
            if apple_wp.f_cursed:
                return
            # Inactive, so remove it (we'll create a new one)
            else:
                apple_wp.Destroy()

        # Makes guard move towards an apple he can see.
        def go_apple(m, x, y):
            activator.Say("Oh, an apple!")

            # Find the currently active waypoint of this guard.
            for wp in activator.FindObjects(type = Type.WAYPOINT_OBJECT):
                # Is it active?
                if wp.f_cursed:
                    # Deactivate it; we'll create a new active waypoint, which will
                    # point to this waypoint as the next one to go to.
                    wp.f_cursed = False

                    # Create the new waypoint.
                    new_wp = activator.CreateObject("waypoint")
                    # Set up the waypoint's ID and the next waypoint to go to.
                    new_wp.name = "apple waypoint"
                    new_wp.title = wp.name
                    # The coordinates
                    new_wp.hp = x
                    new_wp.sp = y
                    new_wp.slaying = m.path
                    # Activate the waypoint.
                    new_wp.f_cursed = True

                    # Create an event object; will be triggered when the guard
                    # reaches the apple waypoint, which will make the guard "eat"
                    # the apple (it will only be removed, really).
                    new_event = CreateObject("event_obj")
                    # Set it up.
                    new_event.sub_type = EVENT_TRIGGER
                    new_event.race = WhatIsEvent().race
                    # Insert it into the new waypoint.
                    new_event.InsertInto(new_wp)
                    break

        # Try to look for an apple around the guard. Squares that he cannot see
        # past or squares with walls are ignored.
        for (m, x, y) in activator.SquaresAround(5, AROUND_BLOCKSVIEW | AROUND_WALL, True):
            for obj in m.GetFirstObject(x, y):
                # Is there an apple?
                if obj.type == Type.FOOD and obj.arch.name == "apple":
                    # Go get it!
                    go_apple(m, x, y)
                    return

main()
