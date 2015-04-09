#!/usr/bin/python3
"""
Main file of the map checker program.
"""

from collections import OrderedDict
import getopt
import os
import queue
import sys
from threading import Thread
import logging
import logging.handlers

from system.checker import CheckerMap, CheckerObject, CheckerArchetype, \
    AbstractChecker
from system.config import Config
import system.constants
from system.game_object import AbstractObjectCollection, ArchObjectCollection, \
    ArtifactObjectCollection, RegionObjectCollection
from system.parser import ParserMap, ParserArchetype, ParserArtifact, \
    ParserRegion
from system.saver import SaverMap
from system.scanner import ScannerMap
from ui.window_main import WindowMain


class MapChecker:
    """
    Implements the map checker class, which handles things such as
    calling the map parser, actually checking for errors, etc.
    """

    definitionFilesData = OrderedDict([
        [
            "archetype",
            {
                "filename": "archetypes",
                "option": "path_dir_arch"
            }
        ],
        [
            "artifact",
            {
                "filename": "artifacts",
                "option": "path_dir_arch"
            }
        ],
        [
            "region",
            {
                "filename": "regions.reg",
                "option": "path_dir_maps"
            }
        ],
    ])

    def __init__(self, config):
        self.config = config

        # Create scanners, datasets, parsers, etc.
        self.scanner = ScannerMap(config)
        self.archetypes = ArchObjectCollection("archetype")
        self.artifacts = ArtifactObjectCollection("artifact")
        self.regions = RegionObjectCollection("region")
        self.saver_map = SaverMap(config)
        self.checker_map = CheckerMap(config)
        self.checker_object = CheckerObject(config)
        self.checker_archetype = CheckerArchetype(config)
        self.checker_artifact = self.checker_archetype
        self.parser_map = ParserMap(config)
        self.parser_archetype = ParserArchetype(config)
        self.parser_artifact = ParserArtifact(config)
        self.parser_region = ParserRegion(config)
        self.global_objects = {
            system.constants.game.types.beacon: [],
        }

        for collection in self.collections:
            self.collection_parser(collection).setCollection(collection)

        self.archetypes.addLinkedCollection(self.artifacts)

        for name in dir(self):
            if hasattr(getattr(self, name), "set_map_checker"):
                getattr(self, name).set_map_checker(self)

        self.checker_map.set_map_checker(self)

        self.info_add_fnc = None
        self._thread = None

        self.queue = queue.Queue()

        self._thread_running = False
        self._scan_status = ""
        self._scan_progress = 0

        self.path = os.path.dirname(os.path.realpath(__file__))

    @property
    def collections(self):
        """Returns all the available object collections."""
        return sorted([self.__dict__[obj] for obj in self.__dict__ if
                       isinstance(self.__dict__[obj],
                                  AbstractObjectCollection)],
                      key=lambda x: list(self.definitionFilesData.keys()).index(
                          x.name))

    @property
    def checkers(self):
        """Returns all the available checkers."""
        return [self.__dict__[obj] for obj in self.__dict__ if
                isinstance(self.__dict__[obj], AbstractChecker)]

    def collection_parser(self, collection):
        """Returns a parser object for the specified collection."""
        return self.__dict__["parser_" + collection.name]

    def collection_checker(self, collection):
        """Returns a checker object for the specified collection."""

        try:
            return self.__dict__["checker_" + collection.name]
        except KeyError:
            return None

    def get_definitions_path(self, name):
        """Returns absolute path to the specified definitions file."""

        try:
            path = self.definitionFilesData[name]["path"]
        except KeyError:
            path = self.config.get("General",
                                   self.definitionFilesData[name]["option"])

        return os.path.join(path, self.definitionFilesData[name]["filename"])

    def get_maps_path(self):
        """Returns absolute path to the maps directory."""
        return self.config.get("General", "path_dir_maps")

    def get_server_path(self):
        """Returns absolute path to the server directory."""
        return self.config.get("General", "path_dir_server")

    def checkers_set_fix(self, fix):
        """Set the fix attribute for all checkers."""
        for checker in self.checkers:
            checker.fix = fix

    def _scan(self, path, files, rec, fix):
        """
        Internal function for actually performing the scan. Used by scan,
        in both threading and non-threading mode. Changes things such as
        _scan_status, _scan_progress, etc. Puts info about errors into
        the queue object
        """

        self.checkers_set_fix(fix)

        if not path:
            path = self.get_maps_path()

        for key in self.global_objects:
            self.global_objects[key] = []

        self._scan_progress = 0

        if not files:
            # First scan for possible map files.
            self._scan_status = "Gathering files..."
            files = self.scanner.scan(path, rec)

        # Now filter out non-map files by reading the header of all
        # found files.
        self._scan_status = "Gathering map files..."
        maps = self.scanner.filter_map_files(files)

        # Parse file definitions.
        for collection in self.collections:
            if not self._thread_running:
                return

            path = self.get_definitions_path(collection.name)
            checker = self.collection_checker(collection)

            if collection.needReload(path):
                self._scan_status = "Parsing {} definitions...".format(
                    collection.name)

                with open(path, "r") as f:
                    self.collection_parser(collection).parse(f)
                    collection.setLastRead(path)

                for error in self.collection_parser(collection).errors:
                    self.queue.put(error)

                if checker:
                    self._scan_status = "Checking {} definitions...".format(
                        collection.name)
                    checker.setPath(path)

                    for obj in collection:
                        checker.check(collection[obj])

                        for error in checker.errors:
                            self.queue.put(error)
                            self.collection_parser(collection).errors.append(
                                error)
            else:
                for error in self.collection_parser(collection).errors:
                    self.queue.put(error)

        # Now loop through the maps.
        for i, file in enumerate(maps):
            if not self._thread_running:
                return

            # Update scan progress.
            self._scan_progress = (i + 1) / len(maps)

            self._scan_status = "Parsing {}...".format(file)

            # Parse the map file.
            with open(file, "r") as f:
                m = self.parser_map.parse(f)

            self._scan_status = "Checking {}...".format(file)

            self.checker_map.check(m)

            if m.isModified():
                with open(file + ".tmp", "w", newline="\n") as f:
                    self.saver_map.save(m, f)

                os.unlink(file)
                os.rename(file + ".tmp", file)

            for error in self.checker_map.errors:
                self.queue.put(error)

    def scan(self, path=None, files=None, rec=True, fix=False,
             threading=True):
        """Perform a new scan for errors."""
        if self._thread and self._thread.is_alive():
            return

        self._thread_running = True

        if threading:
            self._thread = Thread(target=self._scan,
                                  args=(path, files, rec, fix))
            self._thread.start()
        else:
            self._scan(path, files, rec, fix)

    def scan_stop(self):
        """Stop current scan, if any."""
        if self._thread and self._thread.is_alive():
            self._thread_running = False
            self._scan_status = "Stopping..."

    def scan_is_running(self):
        """Check if there is currently a scan running."""
        if self._thread:
            return self._thread.is_alive()

        return False

    def scan_get_progress(self):
        """Get progress of the current scan."""
        return self._scan_progress

    def scan_get_status(self):
        """Get status of the current scan."""
        return self._scan_status

    def exit(self):
        """Called when the application exits. Stops the scan, if any."""
        self.scan_stop()


def excepthook(exc_type, exc_value, exc_tback):
    logger = logging.getLogger("interface-editor")
    logger.error("Logging an uncaught exception",
                 exc_info=(exc_type, exc_value, exc_tback))
    sys.__excepthook__(exc_type, exc_value, exc_tback)


def main():
    from PyQt5.QtWidgets import QApplication

    sys.excepthook = excepthook

    logger = logging.getLogger('interface-editor')
    logger.setLevel(logging.DEBUG)
    formatter = logging.Formatter(
        "%(asctime)s - %(name)s - %(levelname)s - %(message)s")

    handler = logging.handlers.RotatingFileHandler(filename='map-checker.log',
                                                   maxBytes=1000 * 1000 * 10,
                                                   backupCount=5)
    handler.setLevel(logging.DEBUG)
    handler.setFormatter(formatter)

    logger.addHandler(handler)

    config = Config()

    # Create a MapChecker class.
    map_checker = MapChecker(config)
    # Load the program's configuration.
    config.load(map_checker.path)

    # Try to parse our command line options.
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hcfd:m:a:r:",
                                   ["help", "cli", "fix", "directory=",
                                    "map=", "arch=", "regions=", "text-only"])
    except getopt.GetoptError as err:
        # Invalid option, show the error and exit.
        print(err)
        sys.exit(2)

    cli = False
    fix = False
    files = []
    path = None

    # Parse options.
    for o, a in opts:
        if o in ("-h", "--help"):
            # usage()
            sys.exit()
        elif o in ("-c", "--cli"):
            cli = True
        elif o in ("-f", "--fix"):
            fix = True
        elif o in ("-d", "--directory"):
            path = a
        elif o in ("-m", "--map"):
            files.append(a)
        elif o in ("-a", "--arch"):
            # TODO: make this more robust?
            map_checker.definitionFilesData["archetype"]["path"] = a
            map_checker.definitionFilesData["artifact"]["path"] = a
        elif o in ("-r", "--regions"):
            # TODO: maps directory option instead?
            a = os.path.dirname(a)
            map_checker.definitionFilesData["region"]["path"] = a

    if not cli:
        # Create a GUI window using Qt.
        app = QApplication(sys.argv)
        window = WindowMain()
        window.setMapChecker(map_checker)
        window.set_config(config)
        window.setExitFunction(map_checker.exit)
        window.show()

        ret = app.exec_()
    else:
        map_checker.scan(path=path, files=files, fix=fix,
                         threading=False)
        ret = 0

        while map_checker.queue.qsize():
            try:
                error = map_checker.queue.get(0)
                severity = "<b>{}</b>".format(error["severity"].upper())
                desc = error["description"]

                if error["explanation"]:
                    desc += "<br>"
                    desc += format(error["explanation"])

                l = [severity, desc]

                if error["loc"]:
                    l.insert(0, " ".join(str(i) for i in error["loc"]))

                print(" ".join(l))
            except queue.Empty:
                pass

    # Save configuration on exit.
    config.save()
    sys.exit(ret)


if __name__ == "__main__":
    main()
