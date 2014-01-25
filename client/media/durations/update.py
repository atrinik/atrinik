#!/usr/bin/env python

import sys, os

try:
    import gst, gst.pbutils
except:
    print("Could not import py-gst, make sure it's installed (Debian: python-gst0.10 package)")
    sys.exit(1)

class GSTFileInfo:
    def __init__(self):
        pass

    def examine_file(self, file):
        newitem = gst.pbutils.Discoverer(50000000000)

        try:
            self.info = newitem.discover_uri("file://" + file)
        except:
            return False

        return True

    def get_duration(self):
        return self.info.get_duration() / 1000000000.0

def main():
    print("Updating durations database...")

    currdir = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(currdir, "..")
    info = GSTFileInfo()

    for node in os.listdir(path):
        fullpath = os.path.join(path, node)

        if os.path.isfile(fullpath):
            if not info.examine_file(fullpath):
                continue

            duration = int(info.get_duration())

            if duration:
                fp = open(os.path.join(currdir, node), "w")
                fp.write("{}".format(duration))
                fp.close()

    print("Done!")

if __name__ == "__main__":
    main()
