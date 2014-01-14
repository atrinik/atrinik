'''
Main file of the map checker program.
'''

from collections import OrderedDict
import getopt
import os
import queue
import sys
from threading import Thread
import time

from system.checker import CheckerMap, CheckerObject, CheckerArchetype
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
    '''
    Implements the map checker class, which handles things such as
    calling the map parser, actually checking for errors, etc.
    '''
    
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
        
        self._scan_running = False
        self._scan_status = ""
        self._scan_progress = 0
        
        self.path = os.path.dirname(os.path.realpath(__file__))
        
    @property
    def collections(self):
        '''Returns all the available object collections.'''
        return sorted([self.__dict__[obj] for obj in self.__dict__ if isinstance(self.__dict__[obj], AbstractObjectCollection)], key = lambda x: list(self.definitionFilesData.keys()).index(x.name))
    
    def collection_parser(self, collection):
        '''Returns a parser object for the specified collection.'''
        return self.__dict__["parser_" + collection.name]
    
    def collection_checker(self, collection):
        '''Returns a checker object for the specified collection.'''
        
        try:
            return self.__dict__["checker_" + collection.name]
        except KeyError:
            return None
    
    def getDefinitionsPath(self, name):
        '''Returns absolute path to the specified definitions file.'''
        return os.path.join(self.config.get("General", self.definitionFilesData[name]["option"]), self.definitionFilesData[name]["filename"])
    
    def getMapsPath(self):
        '''Returns absolute path to the maps directory.'''
        return self.config.get("General", "path_dir_maps")
        
    def _scan(self, path):
        '''
        Internal function for actually performing the scan. Used by scan,
        in both threading and non-threading mode. Changes things such as
        _scan_status, _scan_progress, etc. Puts info about errors into
        the queue object
        '''
        
        if not path:
            path = self.getMapsPath()
            
        for key in self.global_objects:
            self.global_objects[key] = []
        
        self._scan_progress = 0
        
        # First scan for possible map files.
        self._scan_status = "Gathering files..."
        files = self.scanner.scan(path)
        
        # Now filter out non-map files by reading the header of all
        # found files.
        self._scan_status = "Gathering map files..."
        maps = self.scanner.filter_map_files(files)
        
        # Parse file definitions.
        for collection in self.collections:
            if not self._thread_running:
                return
            
            path = self.getDefinitionsPath(collection.name)
                
            if collection.needReload(path):
                self._scan_status = "Parsing {} definitions...".format(collection.name)
                
                with open(path, "r") as f:
                    self.collection_parser(collection).parse(f)
                    collection.setLastRead(path)
                    
                checker = self.collection_checker(collection)
                
                if checker:
                    self._scan_status = "Checking {} definitions...".format(collection.name)
                    checker.setPath(path)
                    
                    for obj in collection:
                        checker.check(collection[obj])
                        
                        for error in checker.errors:
                            self.queue.put(error) 
            
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
                
            #with open(file + ".tmp", "w", newline="\n") as f:
            #    self.saver_map.save(m, f)
            
            self._scan_status = "Checking {}...".format(file)
            
            self.checker_map.check(m)
            
            for error in self.checker_map.errors:
                self.queue.put(error)
        
    def scan(self, path = None, threading = True):
        '''Perform a new scan for errors.'''
        if self._thread and self._thread.is_alive():
            return
        
        self._thread_running = True
            
        if threading:
            self._thread = Thread(target = self._scan, args = (path, ))
            self._thread.start()
        else:
            self._scan(path)
    
    def scan_stop(self):
        '''Stop current scan, if any.'''
        if self._thread and self._thread.is_alive():
            self._thread_running = False
            self._scan_status = "Stopping..."
            
    def scan_is_running(self):
        '''Check if there is currently a scan running.'''
        if self._thread:
            return self._thread.is_alive()
        
        return False
    
    def scan_get_progress(self):
        '''Get progress of the current scan.'''
        return self._scan_progress
    
    def scan_get_status(self):
        '''Get status of the current scan.'''
        return self._scan_status
            
    def exit(self):
        '''Called when the application exits. Stops the scan, if any.''' 
        self.scan_stop()

def main():
    from PyQt5.QtWidgets import QApplication
    
    config = Config()
    
    # Create a MapChecker class.
    map_checker = MapChecker(config)
    # Load the program's configuration.
    config.load(map_checker.path)
    
    # Try to parse our command line options.
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hc", ["help", "cli"])
    except getopt.GetoptError as err:
        # Invalid option, show the error and exit.
        print(err)
        sys.exit(2)
        
    cli = False
    
    # Parse options.
    for o, a in opts:
        if o in ("-h", "--help"):
            ##usage()
            sys.exit()
        elif o in ("-c", "--cli"):
            cli = True 
    
    if not cli:
        # Create a GUI window using Qt.
        app = QApplication(sys.argv)
        window = WindowMain()
        window.set_config(config)
        window.setMapChecker(map_checker)
        window.setExitFunction(map_checker.exit)
        window.show()
        
        ret = app.exec_()
    else:
        map_checker.scan(threading = False)    
        ret = 0
        
    # Save configuration on exit.
    config.save()
    sys.exit(ret)
    
if __name__ == "__main__":
    main()
