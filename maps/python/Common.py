## @file
## This file holds common code used across Atrinik Python scripts.

import re

## Calculate the diagonal distance between two X and Y coordinates.
def diagonal_distance(x1, y1, x2, y2):
    return max(abs(x1 - x2), abs(y1 - y2))

## Computes an absolute direction.
## @param d Direction to convert.
## @return Number between 1 and 8, which represents the "absolute" direction
## of a number (it actually takes care of "overflow" in previous calculations
## of a direction). */
def absdir(d):
    while d < 1:
        d += 8

    while d > 8:
        d -= 8

    return d

## Returns a random direction (1..8) similar to a given direction.
## @param d The exact direction.
## @return The randomized direction. */
def get_randomized_dir(d):
    from random import randint
    return absdir(d + randint(0, 2) + randint(0, 2) - 2)

## Removes extraneous whitespace and unprintable characters from player's
## text input.
def player_sanitize_input(s):
    if not s:
        return None

    import string
    return "".join("".join(c for c in s if c in string.printable).split(" "))

## Search squares around the activator, looking for an object identified
## by its name/count.
## @param activator The activator. Will search below and around this object.
## @param limit Maximum number of squares from the activator to search.
## @param archname Optional archname to look for.
## @param name Optional name to look for.
## @param count Optional object ID to look for.
def find_obj(activator, limit = 10, archname = None, name = None, count = None):
    if archname == None and name == None and count == None:
        raise AttributeError("No matching conditions provided.")

    for (m, x, y) in [(activator.map, activator.x, activator.y)] + activator.SquaresAround(limit):
        first = m.GetFirstObject(x, y)

        if not first:
            continue

        toscan = [first]

        while toscan:
            scanning = toscan.pop()

            for tmp in scanning:
                if tmp.inv:
                    toscan.append(tmp.inv)

                if archname != None and tmp.arch.name != archname:
                    continue

                if name != None and tmp.name != name:
                    continue

                if count != None and tmp.count != count:
                    continue

                return tmp

def obj_assign_attribs (obj, attribs):
    if not attribs:
        return

    for (attrib, val) in re.findall(r'(\w+) ("[^"]+"|[^ ]+)', attribs):
        if val.startswith('"') and val.endswith('"'):
            val = val[1:-1]

        # Try to create an integer or a float from the value if possible.
        try:
            val = int(val)
        except ValueError:
            try:
                val = float(val)
            except ValueError:
                pass

        # Translate None string to literal None
        if val == "None":
            val = None

        # If the object has the attribute, set it directly
        if hasattr(obj, attrib):
            setattr(obj, attrib, val)
        # If the object has the flag attribute, set it directly
        elif hasattr(obj, "f_" + attrib):
            setattr(obj, "f_" + attrib, True if val else False)
        # Otherwise attempt to use the Load method
        else:
            obj.Load("{} {}".format(attrib, "NONE" if val == None else val))

