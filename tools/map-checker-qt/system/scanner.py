"""
Implements functions related to scanning the filesystem for various purposes.
"""

import os

from system import parser


class ScannerMap:
    """
    Implements class that provides scanning for map files.
    """

    def __init__(self, config):
        self.config = config

    def scan(self, path, rec=True):
        """
        Scans the specified path for map files. Note that this only guesses
        what are map files and what are not. Use filter_map_files to
        thoroughly filter out non-map files.
        """
        files = []

        for file in os.listdir(path):
            filepath = os.path.join(path, file)

            if os.path.isdir(filepath) and rec:
                # Ignore events directories if we are configured to do so.
                if file == "events" and self.config.getboolean(
                        "Filters", "ignore_event_maps"):
                    continue

                # Ignore common non-map directories
                if file in ("styles", "python"):
                    continue

                files += self.scan(filepath)
            elif os.path.isfile(filepath):
                if file.find(".") != -1:
                    continue

                files.append(filepath)

        return files

    def filter_modified_files(self, files, db):
        ret = []

        for file in files:
            if not db.file_is_modified(file):
                continue

            ret.append(file)

        return ret

    @staticmethod
    def filter_map_files(files):
        """
        Checks that the specified files, are, in fact, map files.
        Returns a new list.
        """
        maps = []

        for file in files:
            with open(file) as f:
                # Attempt to read the map file identifier header.
                # If it's there, this is a map file.
                s = f.read(len(parser.mapFileIdentifier))

                if s == parser.mapFileIdentifier:
                    maps.append(file)

        return maps
