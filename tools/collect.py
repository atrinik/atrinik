#!/usr/bin/python

__author__ = "Alex Tokar"
__copyright__ = "Copyright (c) 2009-2015 Atrinik Development Team"
__credits__ = ["Alex Tokar"]
__license__ = "GPL"
__version__ = "2.0"
__maintainer__ = "Alex Tokar"
__email__ = "admin@atokar.net"

import sys
import os
import getopt
import shutil
import platform

from compilers.interface_compiler import InterfaceCompiler
import compilers
import utils


# Adjust the recursion limit.
sys.setrecursionlimit(50000)
# Various paths.
paths = {
    "root": "../",
}

if any(platform.win32_ver()):
    class Colors(object):
        """Common escape sequences."""

        bold = ""
        underscore = ""
        end = ""
else:
    class Colors(object):
        """Common escape sequences."""

        bold = "\033[1m"
        underscore = "\033[4m"
        end = "\033[0m"


def collect_archetypes():
    """Collect archetypes"""
    compiler = compilers.ArchetypesCompiler(paths=paths)
    compiler.compile()


def collect_images():
    """Collect images."""

    compiler = compilers.ImagesCompiler(paths=paths)
    compiler.compile()


def collect_animations():
    """Collect animations."""

    compiler = compilers.AnimationsCompiler(paths=paths)
    compiler.compile()


def collect_treasures():
    """Collect treasures."""

    compiler = compilers.TreasuresCompiler(paths=paths)
    compiler.compile()


def collect_artifacts():
    """Collect artifacts."""

    compiler = compilers.ArtifactsCompiler(paths=paths)
    compiler.compile()


def collect_interfaces():
    """Collect interfaces."""

    compiler = InterfaceCompiler(paths)
    compiler.compile()


def collect_factions():
    """Collect factions."""

    compiler = compilers.FactionsCompiler(paths=paths)
    compiler.compile()


def usage():
    """Show usage."""

    l = []

    # Get the available collectors.
    for entry in dict(globals()):
        if entry.startswith("collect_"):
            l.append(Colors.bold + entry[8:] + Colors.end)

    l.sort()
    collect_list = ", ".join(l[:-2] + [""]) + " and ".join(l[-2:])

    print("\n{bold}{underscore}Use:{end}{end}\nScript for collecting Atrinik "
          "resources like archetypes, treasures, artifacts, etc.\n\n"
          "{bold}{underscore}Options:{end}{end}"
          "\n\t-h, --help:"
          "\n\t\tDisplay this help."
          "\n\n\t-c {underscore}type{end}, --collect={underscore}type{end}:"
          "\n\t\tWhat to collect. This should be one of {collect_list} "
          "(multiple types can be separated by a comma). The default is to "
          "collect everything. If 'none' is in one of the types, no collection "
          "will be done."
          "\n\n\t-d {underscore}directory{end}, --collect={underscore}"
          "directory{end}:"
          "\n\t\tWhere the root directory of your Atrinik copy is. "
          "The default is '../'."
          "\n\n\t-o {underscore}directory{end}, --out={underscore}"
          "directory{end}:"
          "\n\t\tWhere to copy the (collected) files from the arch directory "
          "to (not recursively).".format(bold=Colors.bold,
                                         underscore=Colors.underscore,
                                         end=Colors.end,
                                         collect_list=collect_list))

# Try to parse our command line options.
try:
    opts, args = getopt.getopt(sys.argv[1:], "hc:d:o:", ["help", "collect=",
                                                         "dir=", "out="])
except getopt.GetoptError as err:
    # Invalid option, show the error, print usage, and exit.
    print(err)
    usage()
    sys.exit(2)

what_collect = []
copy_dest = None

# Parse options.
for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    elif o in ("-c", "--collect"):
        what_collect = []

        for t in a.split(","):
            orig = t.strip()
            t = "collect_" + orig

            if t not in locals() and orig != "none":
                print("No such collect option '{}'.".format(orig))
                usage()
                sys.exit()

            what_collect.append(t)
    elif o in ("-d", "--dir"):
        paths["root"] = a
    elif o in ("-o", "--out"):
        copy_dest = a


def main():
    # Get paths to directories inside the root directory.
    for path in utils.find_files(paths["root"], rec=False, ignore_dirs=False,
                                 ignore_files=True):
        paths[os.path.basename(path)] = path

    if "collect_none" not in what_collect:
        # Nothing was set to collect, so by default we'll collect everything.
        if not what_collect:
            for entry in dict(globals()):
                if entry.startswith("collect_"):
                    what_collect.append(entry)

        # Call the collecting functions.
        for collect in what_collect:
            print("Collecting {}...".format(collect.split("_")[-1]))
            globals()[collect]()

    # Copy all files in the arch directory to specified directory.
    if copy_dest:
        files = utils.find_files(paths["arch"], rec=False)

        for path in files:
            shutil.copyfile(path, os.path.join(copy_dest,
                                               os.path.basename(path)))

print("Starting resource collection...")
main()
print("Done!")
