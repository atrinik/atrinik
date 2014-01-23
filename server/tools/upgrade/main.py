#!/usr/bin/python
#
# Script used to upgrade Atrinik server data files. It is safe to run it
# more than once, since all upgrades will be done only once, as long as
# you keep the config.cfg file in the same directory as the script.
#
# It will also make a gzipped backup of your whole directory when being
# ran, just in case anything goes wrong.

import Upgrader, sys, os, tarfile
from datetime import datetime

try:
    from ConfigParser import ConfigParser
# Python 3.x
except:
    from configparser import ConfigParser

print("Starting Atrinik server data upgrader...")

# We will need some recursion.
sys.setrecursionlimit(50000)

# Default path of server's data files.
server_data_path = "../../data"
# Server dir path.
server_path = "../.."

print("Making a backup of data directory...")
tar = tarfile.open(server_path + "/data_backup_" + datetime.now().strftime("%Y%m%d_%H-%M-%S") + ".tar.gz", "w:gz")
tar.add(server_data_path, os.path.basename(server_data_path))
tar.close()

print("Reading configuration...")
config = ConfigParser()
config.read(['config.cfg'])

if not config.has_section("Upgrades"):
    config.add_section("Upgrades")

print("Traversing files that will be considered for upgrade...")
# Load the traverser, and get our files.
traverser = Upgrader.Traverser(server_data_path)
files = traverser.get_files()

upgrades_path = "upgrades"

print("Running upgrade scripts...")

# Go through upgrade scripts.
for item in sorted(os.listdir(upgrades_path)):
    file = upgrades_path + "/" + item

    if os.path.isfile(file):
        dot_pos = item.find(".")

        if dot_pos != -1:
            # Have we ran this script before? If not, run it now.
            if not config.has_option("Upgrades", item[:dot_pos]) or not config.getboolean("Upgrades", item[:dot_pos]):
                print("\tRunning script: {0}".format(file))
                execfile(file)
                config.set("Upgrades", item[:dot_pos], "true")

print("Saving configuration...")
# Save the config.
with open("config.cfg", "wb") as configfile:
    config.write(configfile)

print("... Finished upgrading!")
