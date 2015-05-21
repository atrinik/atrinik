#!/usr/bin/python
#*************************************************************************
#*            Atrinik, a Multiplayer Online Role Playing Game            *
#*                                                                       *
#*    Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team    *
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

# Script to build Atrinik map maker package.

import sys, os, getopt, shutil, glob, zipfile, re
from distutils import dir_util
from datetime import datetime

# Print usage.
def usage():
    print("\nUse:\nScript to build Atrinik map maker package.\n")
    print("Options:")
    print("\n\t-h, --help:\n\t\tDisplay this help.")
    print("\n\t-v, --version:\n\t\tBe verbose about what is happening.")
    print("\n\t-w, --working:\n\t\tWorking copy. This should have Win32 and GNU/Linux server plugins, server binaries and client binaries.")
    print("\n\t-r, --repo:\n\t\tYour copy of the Atrinik main repository.")
    print("\nExample:")
    print("\n\t{0} --repo ~/atrinik --working ~/atrinik-working".format(sys.argv[0]))

# Try to parse our command line options.
try:
    opts, args = getopt.getopt(sys.argv[1:], "hvw:r:", ["help", "verbose", "working=", "repo="])
except getopt.GetoptError as err:
    # Invalid option, show the error, print usage, and exit.
    print(err)
    usage()
    sys.exit(2)

# The default options.
verbose = False
repo_dir = None
working_dir = None

# Parse options.
for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    elif o in ("-v", "--verbose"):
        verbose = True
    elif o in ("-w", "--working"):
        working_dir = a
    elif o in ("-r", "--repo"):
        repo_dir = a

# Directories must be specified.
if not repo_dir:
    print("ERROR: You have no specified repository directory with --repo. Re-run with --help for help.")
    sys.exit(2)
elif not working_dir:
    print("ERROR: You have no specified working copy directory with --working. Re-run with --help for help.")
    sys.exit(2)

# Dirs don't exist?
if not os.path.exists(repo_dir) or not os.path.isdir(repo_dir):
    print("ERROR: '{0}' does not exist or is not a directory.".format(repo_dir))
    sys.exit(2)
elif not os.path.exists(working_dir) or not os.path.isdir(working_dir):
    print("ERROR: '{0}' does not exist or is not a directory.".format(working_dir))
    sys.exit(2)

# Directories to copy from repo.
dirs_copy = ["server", "client", "editor", "maps", "arch", "tools"]
# Directories to make.
dirs_make = ["server/lib", "server/data", "server/data/players", "server/data/tmp", "server/data/log", "server/data/unique-items"]
# Binaries to copy. First parameter is an expression of what binaries
# to copy, second one is whether they are required or not.
binaries = [
    ["server/atrinik-server", True],
    ["server/atrinik-server.exe", True],
    ["server/*.dll", True],
    ["client/atrinik", True],
    ["client/atrinik.exe", True],
    ["client/*.dll", True],
    ["editor/AtrinikEditor.jar", True],
]

# Common debug printing function. Only prints 'txt'
# if --verbose was set.
# @param txt Text to print.
def debug(txt):
    if verbose:
        print(txt)

# Some information...
print("Repo    dir: {0}".format(repo_dir))
print("Working dir: {0}".format(working_dir))
print("\nBuild in progress...\n")

# If 'atrinik_map_maker' directory exists (previous build failed?), remove it first of all.
if os.path.exists("atrinik_map_maker"):
    debug("Recursively removing old atrinik_map_maker directory...")
    shutil.rmtree("atrinik_map_maker")

# Now create a new one.
debug("Creating atrinik_map_maker directory...")
os.mkdir("atrinik_map_maker")

# Copy default directories.
debug("Copying default directories...")

for d in dirs_copy:
    src = repo_dir + "/" + d
    dst = "atrinik_map_maker/" + d
    debug("    Copying: {0}".format(src))

    # Sanity.
    if not os.path.exists(src) or not os.path.isdir(src):
        print("ERROR: '{0}' does not exist or is not a directory.".format(src))
        sys.exit(2)

    # Copy recursively.
    dir_util.copy_tree(src, dst)

# Create default directories.
debug("Creating default directories...")

for d in dirs_make:
    dst = "atrinik_map_maker/" + d
    debug("    Creating: {0}".format(dst))
    os.mkdir(dst)

# Copy files from server/install_data to server/data.
debug("Copying default files from install_data directory to data directory...")

install_dir = "atrinik_map_maker/server/install_data"

# Sanity check.
if not os.path.exists(install_dir) or not os.path.isdir(install_dir):
    print("ERROR: '{0}' does not exist or is not a directory.".format(install_dir))
    sys.exit(2)

files = os.listdir(install_dir)

for f in files:
    src = install_dir + "/" + f

    # Don't copy directories, only files.
    if os.path.isfile(src):
        debug("    Copying: {0}".format(src))
        # Copy the file.
        shutil.copyfile(src, "atrinik_map_maker/server/data/" + f)

# Now copy other binaries.
debug("Copying binaries...")

for expression in binaries:
    nodes = glob.glob(working_dir + "/" + expression[0])

    if expression[1] and len(nodes) == 0:
        print("ERROR: Could not match expression '{0}', missing file(s)?".format(expression[0]))
        sys.exit(2)

    for src in nodes:
        debug("    Copying: {0}".format(src))
        # Copy the binary.
        shutil.copyfile(src, "atrinik_map_maker/" + src[len(working_dir) + 1:])

# Copy files from 'merge' directory to the atrinik_map_maker dir.
debug("Merging contents of 'merge' directory...")

merge_dir = "merge"

# Sanity.
if not os.path.exists(merge_dir) or not os.path.isdir(merge_dir):
    print("ERROR: '{0}' does not exist or is not a directory.".format(merge_dir))
    sys.exit(2)

files = os.listdir(merge_dir)

for f in files:
    src = merge_dir + "/" + f
    dst = "atrinik_map_maker/" + f
    debug("    Merging: {0}".format(f))

    # For files, just copy it, for dirs, copy the entire tree recursively.
    if os.path.isfile(src):
        shutil.copyfile(src, dst)
    else:
        dir_util.copy_tree(src, dst)

# Recursively add files to zip archive.
# @param zipf ZipFile instance.
# @param directory Directory we're doing.
def zip_recurse(zipf, directory):
    nodes = os.listdir(directory)

    for item in nodes:
        src = directory + "/" + item

        # Write the file to the zip archive.
        if os.path.isfile(src):
            zipf.write(src, None, zipfile.ZIP_DEFLATED)
        # Go on recursively.
        elif os.path.isdir(src):
            zipf.write(src, None, zipfile.ZIP_DEFLATED)
            zip_recurse(zipf, src)

# Find Atrinik's version.
debug("Attempting to find Atrinik's version...")

version = ""

# Client's build.config file.
client_build_config = repo_dir + "/client/build.config"

# Does it exist?
if os.path.exists(client_build_config):
    fp = open(client_build_config)

    for line in fp:
        match = re.match(".*PACKAGE_VERSION_\w+ (\d+).*", line)

        if match:
            version += match.group(1) + "."

    fp.close()

    if version:
        version = version[:-1] + "_"
        debug("    Found Atrinik's version: {0}".format(version[:-1]))

# Zip the entire dir.
debug("Zipping up the entire directory...")

# The zip archive name.
zip_name = "atrinik_map_maker_" + version + datetime.now().strftime("%Y%m%d") + ".zip"

# Create the zip archive.
zipf = zipfile.ZipFile(zip_name, "w", compression = zipfile.ZIP_DEFLATED)
# Now recursively add the map maker dir.
zip_recurse(zipf, "atrinik_map_maker")
# Close the archive.
zipf.close()

# Remove the map maker dir.
debug("Recursively removing temporary directory...")
shutil.rmtree("atrinik_map_maker")

# Finished!
print("  Done!")
