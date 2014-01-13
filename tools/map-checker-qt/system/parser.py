'''
Implements parsing of various files, such as maps, archetypes, artifacts, etc.
'''

import os
from system.game_object import GameObject, MapObject, ArchObject, ArtifactObject, RegionObject

# File header that identifies map files.
mapFileIdentifier = "arch map\n"

class Parser:
    '''
    General-use parser. Implements logic that allows it to parse game
    objects in archetypes, map files, etc. Works recursively, so
    inventories are parsed correctly as well.
    
    In general, the handle_line function can be used to handle a line
    in a file, and put its properties into the specified object as
    attributes. However, this does not handle inventories, so using
    _parse in some way or another is generally recommended.
    '''
    
    # Object start identification, for example, in 'arch chair ... end'.
    objectIdentifiers = ["arch"]
    
    def __init__(self, config):
        self.config = config
        
        # Used to handle multi-part object definitions in archetypes
        self.in_more = False
        # Used to handle multi-line strings (msg ... endmsg)
        self.in_msg = False
        self.msg = ""
        
        self.collection = None
        self.errors = []
        
    def addError(self, explanation, line = None, is_map_file = False):
        if line:
            explanation = "{}<br><br><b>Line contents:</b><br>{}".format(explanation, line)
             
        error = {
                 "file": {
                         "name": os.path.basename(self.path).split(".")[0].capitalize(),
                         "path": self.path,
                         "is_map": is_map_file
                         },
                 "severity": "critical",
                 "description": "Parsing error on line {}.".format(self.line_number if line else "???"),
                 "explanation": explanation
        }
        
        self.errors.append(error)
    
    def setCollection(self, collection):
        '''Data structure where objects will be stored.'''
        self.collection = collection
        
    def set_map_checker(self, map_checker):
        self.map_checker = map_checker
        
    def objectLoadedHandler(self, obj):
        '''
        This function is called from inside _parse when an object has been
        successfully loaded, and the 'end' keyword was reached. This allows
        one to do post-processing of the loaded object, such as putting it
        on its appropriate tile in a multi-dimensional map array, for example.
        '''
        
        if self.collection != None:
            self.collection[obj.name] = obj
            
    def objectCreatedHandler(self, obj):
        pass
    
    def handle_line(self, line, obj):
        '''
        Implements handling for one line. This handles things such as
        adding attributes to the specified object (for example, 'x 10'
        or 'name orc slayer'). Also implements handling multi-line
        strings (msg ... endmsg).
        '''
        
        # Handle 'msg ... endmsg'
        if line == "msg\n":
            self.msg = ""
            self.in_msg = True
        elif line == "endmsg\n":
            self.in_msg = False
            
            if obj:
                obj.setAttribute("msg", self.msg[:-1])
            else:
                self.addError("Tried to add attribute, but object definition was missing.", line)
        elif self.in_msg:
            self.msg += line
        # Skip empty and comment lines.
        elif line.strip() == "" or line.startswith("#"):
            pass
        # Mark that this is a multi-part object definition, so that
        # _parse can manage the linking appropriately.
        elif line == "More\n":
            self.in_more = True
        # Everything else is an attribute.
        else:
            attribute = line.split(" ")[0]
            
            if obj:
                obj.setAttribute(attribute, line[len(attribute):].strip())
            else:
                self.addError("Tried to add attribute, but object definition was missing.", line)
            
    def _parse_setup(self, f):
        '''Performs routines/cleanup prior to parsing. Must be called.'''
        self.path = f.name
        self.line_number = 0
        self.errors = []
        
        if self.collection != None:
            self.collection.clear()
    
    def _parse(self, f, obj = None, retval = False, cls = GameObject):
        '''
        Implements general parsing of objects on map, in artifacts
        definitions, archetypes, etc.
        @param f File handle to read from.
        @param obj Object that will receive the parsed data. None means
        that the function will first try looking for a definition, such
        as 'arch bat' or 'Object table' for example.
        @param retval If True, will return the object when done parsing,
        instead of calling objectLoadedHandler on it. Mostly used for
        recursion in this function, to load inventories.
        @param cls What class to initiate objects as.
        '''
        
        # Last processed object.
        last_obj = None
        
        for line in f:
            self.line_number += 1
            space = line.find(" ")
            
            # If there are any object identifiers, try to look for that in the line.
            if self.objectIdentifiers and space != -1 and line[:space] in self.objectIdentifiers:
                name = line[space:].strip()
                newobj = cls(name)
                self.objectCreatedHandler(newobj)
                
                if type(newobj) == GameObject:
                    arch = self.map_checker.archetypes.get(name)
                    
                    if arch == None:
                        self.addError("Unknown archetype: <b>{}</b>".format(name), line)
                    else:
                        newobj.setArch(arch)
                
                # We are already processing an existing object. This means we
                # have found an object that belongs in the previous object's
                # inventory.
                if obj:
                    ret = self._parse(f, newobj, True, cls)
                    
                    if ret:
                        obj.inventoryAdd(ret)
                        ret.setParent(obj)
                    else:
                        self.addError("Failed to load object.", line)
                # Otherwise just create new object.
                else:
                    obj = newobj
                    
                    # If 'More' keyword was found before this, it means we're
                    # processing archetypes. Link this new object to the 
                    # previous object.
                    if self.in_more:
                        obj.setHead(last_obj)
                        last_obj.setMore(obj)
                        self.in_more = False
            # If this is the end of the object's definitions, perform
            # the appropriate handling.
            elif line == "end\n":
                if not obj:
                    self.addError("Found end keyword but there was no object definition preceding it.", line)
                    
                if retval:
                    return obj
                
                if obj:
                    self.objectLoadedHandler(obj)
                    
                last_obj = obj
                obj = None
            else:
                self.handle_line(line, obj)
                
    def parse(self, f):
        '''
        Just a wrapper for _parse. Parsers that inherit this class are
        free to override this method with their own parsing logic, and
        then call _parse directly when needed.
        '''
        self._parse_setup(f)
        self._parse(f)
    
class ParserArchetype(Parser):
    '''Archetype parser.'''
    
    # 'Object beholder' is used to mark an archetype, but
    # 'arch eye_of_beholder' is used to put a beholder eye in
    # its inventory by default. Thus, we need to handle both. 
    objectIdentifiers = ["Object", "arch"]
    
    def parse(self, f):
        '''Perform parsing of the archetype file.'''
        
        self._parse_setup(f)
        self._parse(f, cls = ArchObject) 

class ParserArtifact(Parser):
    '''Artifacts parser.'''
    
    # The artifact definitions have a very unique syntax, and there is
    # no such thing as 'Object ring_of_thieves', for example.
    objectIdentifiers = []
    
    def parse(self, f):
        '''Parse the artifacts file.'''
        
        self._parse_setup(f)
        obj = None
        
        for line in f:
            self.line_number += 1
            
            # Artifact definitions begin with 'Allowed xxx', so we will
            # create a dummy artifact object, which we won't actually
            # add to the collection, but it will be used for gathering
            # the artifact's attributes.
            if line.startswith("Allowed "):
                obj = ArtifactObject("dummy")
            # This is the name of the artifact.
            elif line.startswith("artifact "):
                obj.setName(line[line.find(" "):].strip())
            # 'Object' line marks the start of actual object properties,
            # so we will create an artifact object, and load the
            # properties into it. Afterwards, we will copy the artifact's
            # settings (such as drop chance, difficulty, etc) to a unique
            # dataset of the artifact object.
            elif line == "Object\n" or line.startswith("Object "):
                artifact = super(ParserArtifact, self)._parse(f, ArtifactObject(obj.name), True, ArtifactObject)
                
                arch = self.map_checker.archetypes.get(obj.getAttribute("def_arch"))
                    
                if arch == None:
                    self.addError("Unknown archetype: <b>{}</b>".format(obj.getAttribute("def_arch")), line)
                else:
                    artifact.setArch(arch)
                
                artifact.setArtifactAttributes(obj.attributes)
                self.objectLoadedHandler(artifact)
            else:
                self.handle_line(line, obj)

class ParserMap(Parser):
    '''Map file parser.'''
    
    def objectLoadedHandler(self, obj):
        '''Adds the fully loaded object to the appropriate map tile.'''
        self.map.addObject(obj)
        
    def objectCreatedHandler(self, obj):
        obj.map = self.map
    
    def parse(self, f):
        '''Performs map file parsing.'''
        
        self._parse_setup(f)
        self.map = None
        
        for line in f:
            self.line_number += 1
            
            if line == mapFileIdentifier:
                self.map = MapObject(f.name.replace('\\', '/'))
            elif not self.map:
                return None
            elif line == "end\n":
                break
            else:
                self.handle_line(line, self.map)
        
        # Perform parsing of objects on the map.
        self._parse(f)
        
        return self.map
    
class ParserRegion(Parser):
    '''Region file parser.'''
    
    objectIdentifiers = ["region"]
    
    def parse(self, f):
        self._parse_setup(f)
        self._parse(f, cls = RegionObject)
        
        # Links regions to parents, if any.
        for region in self.collection:
            region = self.collection[region]
            
            if not region.parent:
                continue
            
            try:
                parent = self.collection[region.parent]
            except KeyError:
                self.addError("Region <b>{}</b> (<b>{}</b>) defines region <b>{}</b> as its parent, but no such region exists.".format(region.name, region.getAttribute("longname", ""), region.parent))
                continue
            
            region.setParent(parent)
            parent.inventoryAdd(region)
