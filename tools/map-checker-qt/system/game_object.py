'''
Implements objects such as game objects, map objects, archetypes, etc.
'''

from collections import OrderedDict, UserDict
import os

import system.constants


class AbstractObject:
    '''
    An abstract object, implementing properties shared by some other objects.
    '''

    def __init__(self, name):
        self.name = name
        self._attributes = OrderedDict()
        self.modified = False

    def setModified(self, val=True):
        if val is not True:
            return

        self.modified = True

    def isModified(self):
        return self.modified

    def getAttributes(self):
        return self._attributes.keys()

    def setAttribute(self, attribute, value, modified=True):
        '''Set object's attribute to value.'''
        self.setModified(modified)
        self._attributes[attribute] = str(value)

    def replaceAttribute(self, attribute_old, attribute, value, modified=True):
        '''Replace attribute with the specified one.'''
        self.setModified(modified)

        attributes = self._attributes
        self._attributes = OrderedDict()

        for attr in attributes:
            if attr == attribute_old:
                self._attributes[attribute] = value
            else:
                self._attributes[attr] = attributes[attr]

    def removeAttribute(self, attribute, modified=True):
        '''
        Delete object's attribute. It's an error if the attribute does not
        exist.
        '''
        self.setModified(modified)
        del self._attributes[attribute]

    def getAttribute(self, attribute, default=None):
        '''Get object's attribute, return default if attribute doesn't exist.'''
        try:
            return self._attributes[attribute]
        except KeyError:
            return default

    def getAttributeInt(self, attribute):
        '''Get object's attribute as an integer.'''
        return int(self.getAttribute(attribute, 0))

    def getAttributeFloat(self, attribute):
        '''Get object's attribute as a float.'''
        return float(self.getAttribute(attribute, 0.0))

    def setName(self, name, modified=True):
        '''Change object's name.'''
        self.setModified(modified)
        self.name = name

    def save(self):
        '''Save object's data as human-readable string.'''
        l = []

        for attribute in self._attributes:
            if attribute == "msg":
                l.append(
                    "msg\n{0}\nendmsg\n".format(self._attributes[attribute]))
            else:
                l.append(
                    "{0} {1}\n".format(attribute, self._attributes[attribute]))

        return "".join(l)


class AbstractObjectInventory(AbstractObject):
    '''Abstract object with inventory support functions.'''

    def __init__(self, *args):
        super().__init__(*args)

        # Links for parent object (if any) and inventory objects
        self.env = None
        self.inv = []

    def setModified(self, *args):
        '''
        Set the modified state of an abstract inventory object. The object's
        environment object is also marked as modified, if it exists.
        '''
        super().setModified(*args)

        if self.env is not None:
            self.env.setModified(*args)

    def inventoryAdd(self, obj, modified=True):
        '''Add an object to current object's inventory.'''
        self.setModified(modified)
        self.inv.append(obj)

    def setParent(self, obj, modified=True):
        '''Set this object's parent object.'''
        self.setModified(modified)
        self.env = obj

    def getParentTop(self):
        ret = self

        while ret.env:
            ret = ret.env

        return ret


class GameObject(AbstractObjectInventory):
    '''Game object implementation.'''

    def __init__(self, *args):
        super().__init__(*args)

        self.map = None
        self.arch = None
        self._deleted = False

    def setArch(self, arch):
        self.arch = arch

    def setModified(self, *args):
        super().setModified(*args)

        if self.map is not None:
            self.map.setModified(*args)

    def setAttribute(self, attribute, value, modified=True):
        if self.arch and modified and self.arch.getAttribute(
                attribute) == value:
            if super().getAttribute(attribute) != None:
                self.removeAttribute(attribute)

            return

        super().setAttribute(attribute, value, modified)

    def getAttribute(self, attribute, default=None):
        val = super().getAttribute(attribute, default)

        if val == default and self.arch:
            val = self.arch.getAttribute(attribute, default)

        if val == default and attribute == "name":
            return self.name

        return val

    def isSameArchAttribute(self, attr):
        val1 = self.getAttribute(attr)
        val2 = self.arch.getAttribute(attr)

        if val1 == val2:
            return True

        try:
            if float(val1 or 0) == float(val2 or 0):
                return True
        except (ValueError, TypeError):
            pass

        return False

    @property
    def x(self):
        '''Get X property of this object.'''
        return self.getAttributeInt("x")

    @property
    def y(self):
        '''Get Y property of this object.'''
        return self.getAttributeInt("y")

    def delete(self):
        self.setModified()
        self._deleted = True

    def deleted(self):
        return self._deleted


class MapObject(AbstractObject):
    '''Map object implementation.'''

    def __init__(self, *args):
        super(MapObject, self).__init__(*args)

        self.tiles = OrderedDict()

    def addObject(self, obj, modified=True):
        '''Add object to the map.'''
        self.setModified(modified)

        if not obj.x in self.tiles:
            self.tiles[obj.x] = OrderedDict()

        if not obj.y in self.tiles[obj.x]:
            self.tiles[obj.x][obj.y] = []

        self.tiles[obj.x][obj.y].append(obj)

    @property
    def width(self):
        '''Get map's width.'''
        return self.getAttributeInt("width")

    @property
    def height(self):
        '''Get map's height.'''
        return self.getAttributeInt("height")

    def isWorldMap(self):
        return self.getAttribute("name") == system.constants.game.world_map_name


class ArchObject(GameObject):
    '''Implements an archetype object.'''

    def __init__(self, *args):
        super(ArchObject, self).__init__(*args)

        self.head = None
        self.more = None

    def setHead(self, obj):
        self.head = obj

    def setMore(self, obj):
        '''Link a multi-part object.'''
        self.more = obj


class ArtifactObject(ArchObject):
    '''Implements artifact object.'''


class RegionObject(AbstractObjectInventory):
    @property
    def parent(self):
        return self.getAttribute("parent")


class AbstractObjectCollection(UserDict):
    '''Implements a collection of objects.'''

    def __init__(self, name):
        super().__init__()

        self.name = name
        self.linked_collections = []
        self.path = None
        self.last_mtime = 0

    def get(self, key):
        '''
        Custom get implementation. This supports linked collections, so for
        example, the artifacts collection is linked to the archetypes
        collection, and if you use, for example, archetypes["amulet_copper"],
        it will first search the archetypes for the name 'amulet_copper',
        and if it's not found, it will search for the name in the artifacts
        collection. If the name is not found in any collection, None will be
        returned.
        '''

        try:
            return self[key]
        except KeyError:
            try:
                for collection in self.linked_collections:
                    return collection[key]
            except KeyError:
                pass

        return None

    def addLinkedCollection(self, collection):
        '''Links a collection to this one.'''
        self.linked_collections.append(collection)

    def setLastRead(self, path):
        '''Sets the time that the archetypes file was last parsed.'''
        self.last_read = os.path.getmtime(path)
        self.path = path

    def needReload(self, path):
        '''Checks if the collection needs to be reloaded.'''
        return self.path != path or os.path.getmtime(path) > self.last_read


class ArchObjectCollection(AbstractObjectCollection):
    '''Implements a collection of archetypes.'''
    pass


class ArtifactObjectCollection(AbstractObjectCollection):
    '''Implements a collection of artifacts.'''
    pass


class RegionObjectCollection(AbstractObjectCollection):
    '''Implements a collection of regions.'''
    pass
