#!/usr/bin/python
#
# Script to build Atrinik map maker package.

import sys, os, getopt, shutil, glob, zipfile
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
	["server/atrinik_server", True],
	["server/atrinik_server.exe", True],
	["server/*.dll", False],
	["client/atrinik-client", True],
	["client/atrinik.exe", True],
	["client/*.dll", True],
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

# Copy files from server/install to server/data.
debug("Copying default files from install directory to data directory...")

install_dir = "atrinik_map_maker/server/install"

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

# Edit dmfile so everyone can be a DM.
debug("Allowing anyone to become a DM...")

# Original.
dm_file = "atrinik_map_maker/server/data/dmfile"
# Temporary.
dm_file_tmp = "atrinik_map_maker/server/data/dmfile.tmp"
fp_in = open(dm_file, "r")
fp_out = open(dm_file_tmp, "w")

# Go through the lines in the original, and put them to temporary one.
for line in fp_in:
	# If we find the line we're looking for, uncomment it and write it.
	if line == "#*:*:*\n":
		fp_out.write(line[1:])
	else:
		fp_out.write(line)

fp_in.close()
fp_out.close()

# Remove the original file.
os.remove(dm_file)
# Copy the temporary file to the original one.
shutil.move(dm_file_tmp, dm_file)

# Copy plugin binaries.
debug("Copying plugin binaries...")

src_dir = working_dir + "/server/plugins"
dst_dir = "atrinik_map_maker/server/plugins"

# Find .dll plugins.
files_dll = glob.glob(src_dir + "/*.dll")

# No plugins? Bail out.
if len(files_dll) == 0:
	print("ERROR: No Win32 plugins found: {0}".format(src_dir))
	sys.exit(2)

# Find .so plugins.
files_so = glob.glob(src_dir + "/*.so")

# No plugins? Bail out.
if len(files_so) == 0:
	print("ERROR: No GNU/Linux plugins found: {0}".format(src_dir))
	sys.exit(2)

# All the plugins.
files = files_dll + files_so

for src in files:
	base = os.path.basename(src)
	debug("    Copying: {0}".format(src))
	# Copy each plugin.
	shutil.copyfile(src, dst_dir + "/" + base)

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

# Client's configure.ac file.
client_configure = repo_dir + "/client/make/linux/configure.ac"

# Does it exist?
if os.path.exists(client_configure):
	fp = open(client_configure)

	for line in fp:
		# AC_INIT macro has the version.
		if line[:8] == "AC_INIT(":
			# Split it into parts.
			parts = line[8:-2].split(",")
			# Strip off whitespace and remove leading [ and ending ].
			version = parts[1].strip()[1:-1] + "_"
			debug("    Found Atrinik's version: {0}".format(version[:-1]))
			break

	fp.close()

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
