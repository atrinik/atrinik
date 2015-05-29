#!/usr/bin/python

import sys, os, getopt

# Common escape sequences.
class colors:
    bold = "\033[1m"
    underscore = "\033[4m"
    end = "\033[0m"

# Print usage.
def usage():
    print("\n" + colors.bold + colors.underscore + "Use:" + colors.end + colors.end)
    print("\nAtrinik images license checker application.\n")
    print(colors.bold + colors.underscore + "Options:" + colors.end + colors.end)
    print("\n\t-h, --help:\n\t\tDisplay this help.")
    print("\n\t-v, --verbose:\n\t\tPrint all images without known license.")
    print("\n\t-d " + colors.underscore + "directory" + colors.end + ", --directory=" + colors.underscore + "directory" + colors.end + ":\n\t\tSpecify directory where to start checking licenses (recursively). Default is '.'.")
    print("\n\t-c " + colors.underscore + "contributor" + colors.end + ", --contributor=" + colors.underscore + "GPL" + colors.end + ":\n\t\tOptional contributor to list all images of. This can be a part of the contributor's name, the license it is using, etc.")
    print("\n\t--text-only:\n\t\tDo not use escape sequences for coloring the output.")

# Try to parse our command line options.
try:
    opts, args = getopt.getopt(sys.argv[1:], "hvd:c:", ["help", "verbose", "directory=", "contributor=", "text-only"])
except getopt.GetoptError as err:
    # Invalid option, show the error, print usage, and exit.
    print(err)
    usage()
    sys.exit(2)

# The default values.
path = "."
verbose = False
contributor = None

# Parse options.
for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    elif o in ("-v", "--verbose"):
        verbose = True
    elif o in ("-d", "--directory"):
        path = a
    elif o in ("-c", "--contributor"):
        contributor = a.lower()
    elif o == "--text-only":
        colors.bold = ""
        colors.underscore = ""
        colors.end = ""

if not os.path.exists(path) or not os.path.isdir(path):
    print(colors.bold + "ERROR" + colors.end + ": The path '{0}' doesn't exist or is not a directory.".format(path))
    sys.exit(2)

licensed = {}
unlicensed = []
image_info = {}

# Recursively scans directory for images under the same license.
def dir_recurse(path, license, extra):
    for entry in os.listdir(path):
        entry = os.path.join(dirpath, to_remove, entry)

        # Image, add it to the list.
        if entry.endswith(".png"):
            licensed[license].append(entry)

            if extra:
                image_info[entry] = extra
        # Go on recursively.
        elif os.path.isdir(entry):
            dir_recurse(entry, license, extra)

# Scan for images and license files.
for (dirpath, dirnames, filenames) in os.walk(path):
    # Parse LICENSE file in this directory.
    if "LICENSE" in filenames:
        f = os.path.join(dirpath, "LICENSE")
        fh = open(f, "r")

        for linenum, line in enumerate(fh, 1):
            # License name.
            if line.endswith(":\n"):
                license = line.strip()[:-1]

                if not license in licensed:
                    licensed[license] = []
            # Image or directory name that is covered by the license.
            elif line[:1].isspace() and line[1:]:
                try:
                    parts = line.strip().split()
                    to_remove = parts[0]
                except IndexError:
                    print("Invalid line in file: {0}, line: {1}".format(f, linenum))
                    continue

                extra = None

                # There is more data about the image(s).
                if len(parts) > 1:
                    extra = " ".join(parts[1:])

                to_remove_path = os.path.join(dirpath, to_remove)

                # Remove it from dirnames to walk through recursively in the outermost loop
                # but scan the directory to add the image names.
                if to_remove in dirnames:
                    dirnames.remove(to_remove)
                    dir_recurse(to_remove_path, license, extra)
                # Simple image name.
                elif to_remove in filenames:
                    licensed[license].append(to_remove_path)

                    if extra:
                        image_info[to_remove_path] = extra

                    filenames.remove(to_remove)
                # Invalid entry, drop a warning.
                else:
                    print(colors.bold + "ERROR" + colors.end + ": Invalid file/directory definition: {0} ({1}), line: {2}".format(to_remove, f, linenum))
            # Invalid line.
            elif line != "\n":
                print(colors.bold + "ERROR" + colors.end + ": Invalid line in file: {0}, line: {1}".format(f, linenum))

        # Close the license file.
        fh.close()

    # Anything else not removed by license parsing above is marked as unlicensed.
    for entry in filenames:
        entry = os.path.join(dirpath, entry)

        if entry.endswith(".png"):
            unlicensed.append(entry)

# Show list of unlicensed images in verbose mode.
if verbose:
    print(colors.underscore + "List of unlicensed images:" + colors.end)
    unlicensed.sort()

    for entry in unlicensed:
        print("\t" + entry)

if not contributor:
    print("\n" + colors.underscore + "Image contributors:" + colors.end)
    # Store length of licensed images.
    licensed_len = 0

    # Sort the licenses and output their staits.
    for license in sorted(licensed, key = lambda entry: entry):
        print("\t" + license + ": " + colors.bold + str(len(licensed[license])) + colors.end)
        licensed_len += len(licensed[license])

    # Report total statistics.
    print("\n" + colors.underscore + "Statistics:" + colors.end)
    print("\tUnlicensed: {0}{1}{2}".format(colors.bold, len(unlicensed), colors.end))
    print("\tLicensed: {0}{1}{2}".format(colors.bold, licensed_len, colors.end))

    total = licensed_len + len(unlicensed)

    # Show percentage.
    if total:
        print("\tPercent: {0}{1}%{2}".format(colors.bold, licensed_len * 100 / total, colors.end))
# Look for images by specific contributor only.
else:
    license = None

    # Try to find the contributor.
    for entry in licensed:
        if entry.lower().find(contributor) != -1:
            license = entry
            break

    # No such contributor.
    if not license:
        print(colors.bold + contributor + colors.end + " is not a known contributor.")
        sys.exit(2)

    print("\n" + colors.underscore + "Images by " + license + colors.end + ":")

    # Dump out the images.
    for image in licensed[license]:
        print("\t" + image + (" " + image_info[image] if image in image_info else ""))
