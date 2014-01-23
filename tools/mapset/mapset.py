#!/usr/bin/python
#*************************************************************************
#*            Atrinik, a Multiplayer Online Role Playing Game            *
#*                                                                       *
#*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
#*                                                                       *
#* Fork from Crossfire (Multiplayer game for X-windows).                 *
#*                                                                       *
#* This program is free software; you can redistribute it and/or modify  *
#* it under the terms of the GNU General Public License as published by  *
#* the Free Software Foundation; either version 2 of the License, or     *
#* (at your option) any later version.                                   *
#*                                                                       *
#* This program is distributed in the hope that it will be useful,       *
#* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#* GNU General Public License for more details.                          *
#*                                                                       *
#* You should have received a copy of the GNU General Public License     *
#* along with this program; if not, write to the Free Software           *
#* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
#*                                                                       *
#* The author can be reached at admin@atrinik.org                        *
#*************************************************************************

## @file
## Script to generate tiled mapsets, or extend an already existing mapset.

import os, sys, re, shutil, time, random, getopt

## Similar to C/C++ getline, waits for the provided stream to finish
## a line (or anything else delimited by 'delimiter').
## @param stream Which stream to use.
## @param delimiter What marks the end of line.
## @return The line.
def getline(stream = sys.stdin, delimiter = "\n"):
    def _go():
        while True:
            line = stream.readline()

            if not delimiter in line:
                yield line
            else:
                yield line[:line.index(delimiter)]
                break

    return "".join(_go())

## The options class; ask the user questions about the mapset.
class Options:
    ## Initialize the class.
    ## @param options List of the options.
    def __init__(self, options):
        self._options = options

    ## Ask questions and wait for answers.
    def run(self):
        for (i, [opt_name, opt_value, opt_msg]) in enumerate(self._options):
            # Cannot ask this one.
            if not opt_msg:
                continue

            # Loop until we get valid input.
            while True:
                # Ask the question.
                print(opt_msg.format(self[opt_name]))

                # Handle the input (once getline() stops blocking and has finished input).
                if self._handle_input(opt_name, getline()):
                    break

    ## Get option.
    ## @param opt_name The option name to get.
    ## @return The option's value, None if no such option exists.
    def __getitem__(self, opt_name):
        for opt in self._options:
            if opt[0] == opt_name:
                return opt[1]

        return None

    ## Set an option to new value.
    ##
    ## If the option doesn't exist, it will be created.
    ## @param opt_name The option name.
    ## @param val New value.
    def __setitem__(self, opt_name, val):
        for opt in self._options:
            if opt[0] == opt_name:
                opt[1] = val
                return

        self._options.append([opt_name, val, None])

    ## Handles user's input.
    ## @param opt_name Option name.
    ## @param line The input.
    ## @return True on success, False on error.
    def _handle_input(self, opt_name, line):
        # Clean it up...
        value = line.strip()

        # Filename, remove invalid characters.
        if opt_name == "filename":
            # Replace spaces with underscores, and only allow alphanumeric characters,
            # underscores and hyphens.
            value = re.sub(r"[^a-zA-Z0-9\_\-]", "", value.replace(" ", "_")).lower()
        # Parse size.
        elif opt_name == "size":
            # Should be in the format of <num>x<num2>.
            match = re.match(r"([\d]+)(?:\s+)?x(?:\s+)?([\d]+)", value)

            # Invalid input.
            if not match:
                value = None
            else:
                # Construct a tuple containing the two sizes...
                groups = match.groups()
                value = (int(groups[0]), int(groups[1]))

                # Do not allow either width or height to be 0.
                if not value[0] or not value[1]:
                    value = None

        # No value yet, try to use default.
        if not value:
            value = self[opt_name]

        # Still no value...
        if not value:
            print("Error: Invalid value, please try again.")
            return False

        # Set the new value.
        self[opt_name] = value

        # Name, try to guess the filename.
        if opt_name == "name":
            self["filename"] = re.sub(r"[^a-zA-Z0-9\_\-]", "", value.replace(" ", "_")).lower()

        return True

## Handles X/Y coordinate modifiying based on the direction of tiled map.
class Tiles:
    ## The directions of tiled maps.
    _tiles = [
        (0, 0),
        (0, 1),
        (1, 0),
        (0, -1),
        (-1, 0),
        (1, 1),
        (1, -1),
        (-1, -1),
        (-1, 1),
    ]

    ## Dictionary that maps strings into the actual directions in ::_tiles.
    _directions = {
        "north": 1,
        "east": 2,
        "south": 3,
        "west": 4,
        "northeast": 5,
        "southeast": 6,
        "southwest": 7,
        "northwest": 8,
    }

    ## Initialize the class.
    def __init__(self):
        pass

    ## Get the X/Y coordinate modifier for the specified tiled map
    ## direction.
    ## @param val String of the direction, or integer index inside ::_tiles.
    ## @return The X/Y coordinate modifier.
    def __getitem__(self, val):
        if type(val) == type(int()):
            return self._tiles[val]
        else:
            return self._tiles[self._directions[val]]

    ## Get the number of directions for tiled maps.
    def __len__(self):
        return len(self._tiles)

## Transform integer coordinate into string.
## @param val The integer coordinate.
## @return The coordinate in map naming convention string.
def coordinate_str(val):
    # 0 or higher, simple number.
    if val >= 0:
        return "{num:02d}".format(num = val + 1)

    # Negative uses alphabet, so transform the big number
    # into two letters.
    val = -val - 1
    return "{0}{1}".format(chr(97 + val // 26), chr(97 + val % 26))

## Transform string coordinate in map naming convention into integer.
## @param s The string coordinate.
## @return Integer coordinate.
def coordinate_int(s):
    # It's a digit, simple integer.
    if s.isdigit():
        return int(s) - 1

    # Transform the letters into an integer.
    return -(((ord(s[:1]) - 97) * 26) + ((ord(s[1:]) - 97))) - 1

## Generate maps.
## @param directory What directory we're working in.
## @param start X/Y coordinate modifier.
def generate_maps(directory, start):
    # Get the size.
    (width, height) = options["size"]
    # List of archetypes we can use.
    archetypes = [] if options["archetype"] == "nothing" else options["archetype"].split(",")

    for xt in range(width):
        for yt in range(height):
            x = xt + start[0]
            y = yt + start[1]
            # Construct the new path.
            path = "{0}/{1}_{2}{3}".format(directory, options["filename"], coordinate_str(x), coordinate_str(y))

            # Exists already?
            if os.path.exists(path):
                print("Warning: map {0} already exists, not creating.".format(path))
                continue

            # Write out the map information.
            f = open(path, "wb")
            f.write("arch map\n")
            f.write("name {0}\n".format(options["name"]))
            f.write("msg\n")
            f.write("Created:  {0} {1}\n".format(time.strftime("%Y-%m-%d"), options["author"]))
            f.write("endmsg\n")
            f.write("width 24\n")
            f.write("height 24\n")
            f.write("difficulty {0}\n".format(options["difficulty"]))

            # Region...
            if options["region"] != "none":
                f.write("region {0}\n".format(options["region"]))

            # Outdoor?
            if options["outdoor"][:1] == "y":
                f.write("outdoor 1\n")

            # Darkness.
            if options["darkness"] != "-1":
                f.write("darkness {0}\n".format(options["darkness"]))

            # Background music.
            if options["bg_music"] != "none":
                f.write("bg_music {0}\n".format(options["bg_music"]))

            # Weather.
            if options["weather"] != "none":
                f.write("weather {0}\n".format(options["weather"]))

            f.write("end\n")

            # If list of archetypes was provided, tile the map.
            if archetypes:
                for arch_x in range(24):
                    for arch_y in range(24):
                        f.write("arch {0}\n".format(random.choice(archetypes).strip()))

                        if arch_x:
                            f.write("x {0}\n".format(arch_x))

                        if arch_y:
                            f.write("y {0}\n".format(arch_y))

                        f.write("end\n")

            f.close()

## Connect all maps inside directory.
## @param directory The directory to work in.
def connect_maps(directory):
    for f in os.listdir(directory):
        path = os.path.join(directory, f)

        # Hidden file or not a file.
        if f.startswith(".") or not os.path.isfile(path):
            continue

        # Must match map_name_xxyy format.
        if not re.match("(.+)_(.+){4}", f):
            continue

        # Store the file's base name without coordinates.
        fname = f[:-4]
        # Read the file and store the lines in memory.
        f = open(path, "rb")
        lines = f.readlines()
        f.close()

        # Not a map file, go on.
        if lines[0] != "arch map\n":
            continue

        # Get the integer coordinates.
        coords = path[-4:]
        coord_x = coordinate_int(coords[:2])
        coord_y = coordinate_int(coords[2:])
        # Marks that we have written out the tile path information.
        tile_written = False

        # Open the file and truncate it.
        f = open(path, "wb")

        # Write out the lines.
        for line in lines:
            # Not written yet and this is either the end of the map header
            # or we found a tile_path line.
            if not tile_written and (line == "end\n" or line.startswith("tile_path_")):
                # So we don't write them out twice.
                tile_written = True

                # Write out the tile paths.
                for tile in range(1, len(tiles)):
                    # Construct the base map file name.
                    tiled_map = "{0}{1}{2}".format(fname, coordinate_str(coord_x + tiles[tile][0]), coordinate_str(coord_y + tiles[tile][1]))
                    tiled_path = os.path.join(directory, tiled_map)

                    # The map exists and is a file, so we have a valid tiled map.
                    if os.path.exists(tiled_path) and os.path.isfile(tiled_path):
                        f.write("tile_path_{0} {1}\n".format(tile, tiled_map))

            # Ignore tile_path lines.
            if not line.startswith("tile_path_"):
                f.write(line)

        f.close()

## Print usage.
def usage():
    print("Application to create new mapset, or extend existing one.\n\nOptions:")
    print("\t-h, --help: Show this help.")
    print("\t-e, --extend: Extend mode.")
    print("\t-r path, --reconnect=path: Specify a directory of which to reconnect tiled paths.")

# Try to parse our command line options.
try:
    opts, args = getopt.getopt(sys.argv[1:], "her:", ["help", "extend", "reconnect="])
except getopt.GetoptError as err:
    # Invalid option, show the error, print usage, and exit.
    print(err)
    usage()
    sys.exit(2)

# The default options.
extend = False
reconnect = None

# Parse options.
for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    elif o in ("-e", "--extend"):
        extend = True
    elif o in ("-r", "--reconnect"):
        reconnect = a

## Default questions asked in both modes (extending and creating).
def_questions = [
    ["size", "", "Enter the size of the mapset (for example, '10x5' [10 = width, 5 = height]):"],
    ["author", "", "Enter the author (name that appears in the map's message in /mapinfo):"],
    ["archetype", "nothing", "Enter the (comma delimited) archetype(s) with which to fill the created maps (default: '{0}'):"],
    ["difficulty", "1", "Enter the difficulty used across all created maps (default: '{0}'):"],
    ["darkness", "-1", "Enter the darkness used across all created maps ('-1' = '7' = full light, '0' = full darkness, default: '{0}'):"],
    ["outdoor", "no", "Is the mapset outdoor ('yes'/'no', default: '{0}')?:"],
    ["region", "world", "Enter the region the mapset is in ('world', 'none', default: '{0}'):"],
    ["bg_music", "none", "Enter the background music used across all created maps ('cave.xm', 'none', default: '{0}'):"],
    ["weather", "none", "Enter the weather used across all created maps ('snow', 'none', default: '{0}'):"],
]

# Not extending, so ask questions related to the map creating.
if not extend:
    questions = [
        ["name", "", "Enter the name of the mapset (map name for the created maps, for example, 'Ancient Forest'):"],
        ["filename", "", "Enter the base file name (for example, 'world'; default: '{0}'):"],
    ]
# Extending, we need to know where to start extending and the direction.
else:
    questions = [
        ["name", "", "Enter the name of the mapset (map name for the created maps, for example, 'Ancient Forest'):"],
        ["extend_start", "", "Enter the file name where to start extending (for example, '../../maps/shattered_islands/world_0101'):"],
        ["direction", "", "Direction which to extend into (for example, 'south', 'west', 'northeast', etc):"],
    ]

# Initialize Options.
options = Options(questions + def_questions)
# Initialize Tiles.
tiles = Tiles()

## The main function.
def main():
    # Reconnecting maps, no need to ask questions.
    if reconnect:
        if not os.path.exists(reconnect) or not os.path.isdir(reconnect):
            print("Error: Directory {0} doesn't exist or is not a directory.".format(reconnect))
            return

        connect_maps(reconnect)
        return

    # Ask the user some questions...
    options.run()

    # Extending.
    if not extend:
        directory = "generated-maps"
        start = (0, 0)

        # Remove existing directory.
        if os.path.exists(directory):
            shutil.rmtree(directory)

        os.mkdir(directory)
    else:
        # Get the coordinates of the start map...
        coords = options["extend_start"][-4:]
        coord_x = coordinate_int(coords[:2])
        coord_y = coordinate_int(coords[2:])

        # Store the base name of the start map (without coordinates).
        options["filename"] = os.path.basename(options["extend_start"])[:-5]

        try:
            # Get the tile X/Y modifier, depending on the direction the user wanted.
            (tile_x, tile_y) = tiles[options["direction"]]
        except KeyError:
            print("Error: Invalid direction {0}.".format(options["direction"]))
            return

        # West, modify the starting coordinate by the wanted width.
        if tile_x < 0:
            tile_x *= options["size"][0]

        # South, modify the starting coordinate by the wanted height.
        if tile_y < 0:
            tile_y *= options["size"][1]

        directory = os.path.dirname(options["extend_start"])
        start = (coord_x + tile_x, coord_y + tile_y)

    print("Generating maps...")
    generate_maps(directory, start)
    print("Maps saved to {0}.".format(directory))
    print("Connecting maps...")
    connect_maps(directory)
    print("Done!")

main()
