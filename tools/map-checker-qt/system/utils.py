'''
Utility functions.
'''

from html.parser import HTMLParser
from html import entities
import re
import system

PATTERN_MAPNAME = re.compile(r"((?:(?!_(?:[\d]{1,2}|[a-z])_)[\w_])+)(?:_([\d]{1,2}|[a-z]))?_(\d{2}|[a-z]{2})(\d{2}|[a-z]{2})")

class HTMLTextExtractor(HTMLParser):
    def __init__(self):
        HTMLParser.__init__(self)
        self.result = [ ]

    def handle_data(self, d):
        self.result.append(d)

    def handle_charref(self, number):
        codepoint = int(number[1:], 16) if number[0] in ('x', 'X') else int(number)
        self.result.append(chr(codepoint))

    def handle_entityref(self, name):
        codepoint = entities.name2codepoint[name]
        self.result.append(chr(codepoint))

    def get_text(self):
        return "".join(self.result)

def html2text(html):
    s = HTMLTextExtractor()
    s.feed(html)
    return s.get_text()

class MapCoords (object):
    def __init__ (self, name):
        match = PATTERN_MAPNAME.match(name)

        if not match:
            self.name = name
            self.pos = (1, 1, 0)
        else:
            if match.group(2) is not None:
                level = self.str2coord(match.group(2))
            else:
                level = 0

            self.name = match.group(1)
            self.pos = (self.str2coord(match.group(3)),
                self.str2coord(match.group(4)), level)

    def str2coord (self, coord):
        try:
            return int(coord)
        except ValueError:
            values = []
            ret = 0

            for c in coord:
                values.append(ord(c) - (ord("1") if c.isdigit() else ord("a")))

            ret = values[-1]

            if len(values) == 2:
                if coord.isalpha():
                    i = ord("z") - ord("a")
                else:
                    i = ord("9") - ord("1") + 1

                ret += values[0] * i
            else:
                ret += 1

            if coord.isalpha():
                return -ret

            return ret + 100

    def coord2str (self, coord, simple = False):
        if coord > 0:
            if simple:
                return "{}".format(coord)
            else:
                if coord > 99:
                    one, two = divmod(coord - 99, (ord("z") - ord("a")))
                    return "{}{}".format(chr(ord("a") + one), two)
                else:
                    return "{:02d}".format(coord)
        else:
            if simple:
                return "{}".format(chr(ord("a") - coord - 1))
            else:
                one, two = divmod(-coord, (ord("z") - ord("a")))
                return "{}{}".format(chr(ord("a") + one), chr(ord("a") + two))

    def getTiledName (self, idx):
        ret = [self.name]
        pos = tuple(map(sum, zip(self.pos,
            system.constants.game.tiled_coords[idx])))

        if pos[2] != 0:
            ret.append(self.coord2str(pos[2], True))

        ret.append("{}{}".format(self.coord2str(pos[0]),
            self.coord2str(pos[1])))

        return "_".join(ret)

    def getLevel (self):
        return self.pos[2]
