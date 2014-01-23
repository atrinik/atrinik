#!/usr/bin/python
#
# Script for getting information about command permissions
# of specific (or all) players.

import getopt, sys, os, re

class colors:
    bold = "\033[1m"
    underscore = "\033[4m"
    red = "\033[91m"
    end = "\033[0m"

class patterns:
    cmd_permission = re.compile(r"cmd_permission (.+)\n")

def usage():
    print("\n" + colors.bold + colors.underscore + "Use:" + colors.end + colors.end)
    print("\nScript to print information about players' command permissions.\n")
    print(colors.bold + colors.underscore + "Options:" + colors.end + colors.end)
    print("\n\t-h, --help:\n\t\tDisplay this help.")
    print("\n\t-d " + colors.underscore + "directory" + colors.end + ", --directory=" + colors.underscore + "directory" + colors.end + ":\n\t\tSpecify data directory where player files are. Default is '../data/players'.")
    print("\n\t-p " + colors.underscore + "player" + colors.end + ", --player=" + colors.underscore + "player" + colors.end + ":\n\t\tPlayer to list permissions for. Can be specified multiple times for multiple players.")

def get_cmd_permissions(path):
    perms = []

    try:
        fp = open(path)

        for line in fp:
            if line == "endplst\n":
                break

            match = patterns.cmd_permission.match(line)

            if match:
                command = match.group(1)
                perms.append(command if command.startswith("[") and command.endswith("]") else "/" + command)

        fp.close()
    except IOError as err:
        print(err)

    return perms

try:
    opts, args = getopt.getopt(sys.argv[1:], "hd:p:", ["help", "directory=", "player="])
except getopt.GetoptError as err:
    print(err)
    usage()
    sys.exit(2)

path = "../data/players"
players = []

for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    elif o in ("-d", "--directory"):
        path = a
    elif o in ("-p", "--player"):
        players.append(a.lower())

for (dirpath, dirnames, filenames) in os.walk(path):
    for filename in filenames:
        if filename == "player.dat":
            player_name = os.path.basename(dirpath)

            if not players or player_name in players:
                perms = get_cmd_permissions(os.path.join(dirpath, filename))

                if perms:
                    print(colors.red + player_name.title() + colors.end + ":\t\t" + ", ".join(perms))

