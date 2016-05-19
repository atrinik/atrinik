"""
Utility functions.
"""

from html.parser import HTMLParser
from html import entities
import re

import system.constants


PATTERN_MAPNAME_OLD = re.compile(r"((?:(?!_(?:[\d]{1,2}|[a-z])_)[\w_])+)"
                                 r"(?:_([\d]{1,2}|[a-z]))?_(\d{2}|[a-z]{2})"
                                 r"(\d{2}|[a-z]{2})")
PATTERN_MAPNAME = re.compile(r"((?:(?!_(?:[\d]{1,2}|[a-z])_)[\w_])+)_"
                             r"(?:([\d-]+))_(?:([\d-]+))(?:_([\d-]+))?")


class HTMLTextExtractor(HTMLParser):
    def __init__(self):
        HTMLParser.__init__(self)
        self.result = []

    def handle_data(self, d):
        self.result.append(d)

    def handle_charref(self, number):
        codepoint = int(number[1:], 16) if number[0] in ('x', 'X') else int(
            number)
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


def itersubclasses(cls, _seen=None):
    """
    itersubclasses(cls)

    Generator over all subclasses of a given class, in depth first order.

    >>> list(itersubclasses(int)) == [bool]
    True
    >>> class A(object): pass
    >>> class B(A): pass
    >>> class C(A): pass
    >>> class D(B,C): pass
    >>> class E(D): pass
    >>>
    >>> for cls in itersubclasses(A):
    ...     print(cls.__name__)
    B
    D
    E
    C
    >>> # get ALL (new-style) classes currently defined
    >>> [cls.__name__ for cls in itersubclasses(object)] #doctest: +ELLIPSIS
    ['type', ...'tuple', ...]
    """

    if not isinstance(cls, type):
        raise TypeError('itersubclasses must be called with '
                        'new-style classes, not %.100r' % cls)
    if _seen is None:
        _seen = set()

    try:
        subs = cls.__subclasses__()
    except TypeError:  # fails only when cls is type
        subs = cls.__subclasses__(cls)

    for sub in subs:
        if sub not in _seen:
            _seen.add(sub)
            yield sub

            for sub2 in itersubclasses(sub, _seen):
                yield sub2


class MapCoords(object):
    def __init__(self, name):
        match = PATTERN_MAPNAME.match(name)
        match2 = PATTERN_MAPNAME_OLD.match(name)
        self.old_style = False

        if not match and not match2:
            self.name = name
            self.pos = (1, 1, 0)
        elif match:
            self.name = match.group(1)
            level = match.group(4) or 0
            self.pos = (int(match.group(2)), int(match.group(3)), int(level))
        else:
            self.old_style = True
            if match2.group(2) is not None:
                level = self.str2coord(match2.group(2))
            else:
                level = 0

            self.name = match2.group(1)
            self.pos = (self.str2coord(match2.group(3)),
                        self.str2coord(match2.group(4)), level)

    @staticmethod
    def str2coord(coord):
        try:
            return int(coord)
        except ValueError:
            values = []

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

    @staticmethod
    def coord2str(coord, simple=False, old_style=False):
        if not old_style:
            return "{}".format(coord)

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

    def getTiledName(self, idx):
        ret = [self.name]
        pos = tuple(map(sum, zip(self.pos,
                                 system.constants.Game.tiled_coords[idx])))

        if self.old_style:
            if pos[2] != 0:
                ret.append(self.coord2str(pos[2], True))

            ret.append("{}{}".format(self.coord2str(pos[0]),
                                     self.coord2str(pos[1])))
        else:
            ret.append(self.coord2str(pos[0], old_style=self.old_style))
            ret.append(self.coord2str(pos[1], old_style=self.old_style))
            if pos[2] != 0:
                ret.append(self.coord2str(pos[2], old_style=self.old_style))

        return "_".join(ret)

    def getLevel(self):
        return self.pos[2]
