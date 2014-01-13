'''
Implements classes that save objects such as game objects, maps, etc.

This makes it possible to implement auto-fixing.
'''

class Saver:
    '''Generic saver for game objects.'''
    def __init__(self, config):
        self.config = config
    
    def _save(self, obj, f):
        '''
        Internal function for recursively saving an object, including any
        inventories it may have.'''
        f.write("arch {0}\n".format(obj.name))
        f.write(obj.save())
        
        for obj2 in obj.inv:
            self._save(obj2, f)
        
        f.write("end\n")
    
    def save(self, obj, f):
        '''Saves an object to the specified file handle. Uses _save.'''
        self._save(obj, f)
    
class SaverMap(Saver):
    '''Saver for map files. Saves map header and all objects on the map.'''
    def save(self, m, f):
        f.write("arch map\n")
        f.write(m.save())
        f.write("end\n")
        
        for x in range(m.width):
            for y in range(m.height):
                try:
                    for obj in m.tiles[x][y]:
                        super(SaverMap, self).save(obj, f)
                except KeyError:
                    pass
                
                    