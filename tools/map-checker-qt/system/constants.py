'''
Implements general use constants.
'''

class ErrorLevel:
    '''
    Implements the error level class; used for defining an error level,
    and its settings, such as color, etc.
    '''

    def __init__(self, name):
        self.name = name
        self.colors = {}

    def setColor(self, colorType, value):
        '''Sets the error level's color.'''
        self.colors[colorType] = value

    def getColor(self, colorType):
        '''Acquire error level's color.'''
        return self.colors[colorType]

class ErrorLevelCollection:
    '''Collection of error levels.'''

    def __init__(self):
        self.warning = ErrorLevel("warning")
        self.low = ErrorLevel("low")
        self.medium = ErrorLevel("medium")
        self.high = ErrorLevel("high")
        self.critical = ErrorLevel("critical")
        self.fixed = ErrorLevel("fixed")

        self.warning.setColor("qt", "#000000")
        self.low.setColor("qt", "#FF00FF")
        self.medium.setColor("qt", "#00FFFF")
        self.high.setColor("qt", "#0000FF")
        self.critical.setColor("qt", "#ff0000")
        self.fixed.setColor("qt", "#00ff00")

    def __getitem__(self, key):
        return self.__dict__[key] if isinstance(self.__dict__[key], ErrorLevel) else None

class urls:
    '''URLs to links.'''

    report_bug = "http://bugzilla.atrinik.org/"

class game:
    '''Game related constants.'''

    max_level = 115
    world_map_name = "World"
    plugins = ["Python", "Arena"]
    # Highest layer value any archetype can have.
    max_layers = 7
    # Number of sub-layers.
    num_sub_layers = 5

    class types:
        spawn_point = 81
        scroll = 111
        potion = 5
        monster = 80
        spawn_point_mob = 83
        random_drop = 102
        quest_container = 120
        ability = 110
        waypoint = 119
        player = 1
        exit = 66
        teleporter = 41
        floor = 71
        shop_floor = 68
        event_object = 118
        beacon = 126
        sign = 98
        creator = 42
        map_event_object = 127
        wall = 77
        magic_mirror = 28
        door = 20
        gate = 91
        book = 8
        magic_ear = 29
        light_source = 74
        bow = 14
        god = 50
        shop_mat = 69
        cone = 88
        bullet = 2
        rod = 3
        lightning = 12

    class quest_container_sub_types:
        none = 0
        kill = 1
        item = 2
        item_drop = 3
        special = 4
