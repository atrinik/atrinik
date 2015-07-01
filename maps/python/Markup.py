## @file
## Deals with markup-related operations.

import re

from Atrinik import *


## The markup escape table.
_markup_escape_table = {
    "]": "&rsqb;",
    "[": "&lsqb;",
}
## The markup unescape table.
_markup_unescape_table = {
    "[]": "[",
    "&rsqb;": "]",
    "&lsqb;": "[",
}
_markup_unescape_expr = re.compile("({0})".format("|".join(re.escape(s) for s
        in _markup_unescape_table)))

## Escapes markup in the specified string.
## @param text The string.
## @return Escaped string.
def markup_escape(text):
    return "".join(_markup_escape_table.get(c, c) for c in text)

## Unescapes markup in the specified string.
## @param text The string.
## @return Unescaped string.
def markup_unescape(text):
    return _markup_unescape_expr.sub(lambda match:
            _markup_unescape_table[match.group(1)], text)

class Map2Markup:
    def __init__(self, m, x, y):
        self._map = m
        self._x = x
        self._y = y
        self._range = 17

    def set_range(self, r):
        self._range = r

    def _calculate_coords(self):
        self._coords = []

        for x in range(self._range):
            for y in range(self._range):
                self._coords.append((self._x - (self._range // 2) + x, self._y - (self._range // 2) + y))

    def _calculate_mmirrors(self):
        self._mmirrors = {}

        for (x, y) in self._coords:
            for obj in self._get_objects(x, y, LAYER_SYS, -1):
                if obj.type == Type.MAGIC_MIRROR:
                    self._mmirrors[(x, y)] = obj
                    break

    def _calculate_stretch(self, x, y, sub_layer):
        squares = [0] * (SIZEOFFREE1 + 1)

        for i in range(len(squares)):
            squares[i] = self._square_height(x + freearr_x[i], y + freearr_y[i], LAYER_FLOOR, sub_layer)

        for i in range(1, len(squares)):
            if abs(squares[0] - squares[i]) > (8 if i % 2 == 1 else 12):
                squares[i] = squares[0]

        top = max(squares[WEST], squares[NORTHWEST], squares[NORTH], squares[0])
        bottom = max(squares[SOUTH], squares[SOUTHEAST], squares[EAST], squares[0])
        right = max(squares[NORTH], squares[NORTHEAST], squares[EAST], squares[0])
        left = max(squares[WEST], squares[SOUTHWEST], squares[SOUTH], squares[0])
        min_ht = min(top, bottom, left, right, squares[0])
        top -= min_ht
        bottom -= min_ht
        left -= min_ht
        right -= min_ht
        return bottom + (left << 8) + (right << 16) + (top << 24)

    def _square_height(self, x, y, layer, sub_layer):
        z = 0
        floor = self._get_object(x, y, layer, sub_layer)

        if floor:
            z += floor.z

        if z and self._mmirror:
            z += self._mmirror.last_eat

        return z

    def _square_height_top(self, x, y):
        top = 0

        for obj in self._get_objects(x, y, LAYER_FLOOR, -1):
            z = obj.z

            if z and self._mmirror:
                z += self._mmirror.last_eat

            if z > top:
                top = z

        return top

    def _hide_objects(self):
        self._hidden_objects = []

        for (x, y) in reversed(self._coords):
            for layer in range(LAYER_FLOOR, NUM_LAYERS + 1):
                for obj in self._get_objects(x, y, layer, -1):
                    if (obj.more or obj.head != obj) and not obj in self._hidden_objects:
                        tmp = obj.head

                        while tmp:
                            if tmp != obj:
                                self._hidden_objects.append(tmp)

                            tmp = tmp.more

    def _render(self, x, y, obj):
        if not obj or obj.f_hidden or obj in self._hidden_objects:
            return

        darkness = self._map.GetDarkness(x, y)

        if darkness <= 0:
            return

        quick_pos = obj.quick_pos
        obj = obj.head

        if obj.f_draw_direction:
            if obj.direction in (0, NORTH, NORTHEAST, SOUTHEAST, SOUTH, SOUTHWEST, NORTHWEST) and not (x <= self._x and y <= self._y) and not (x > self._x and y < self._y):
                return

            if obj.direction in (0, NORTHEAST, EAST, SOUTHEAST, SOUTHWEST, WEST, NORTHWEST) and not (x <= self._x and y <= self._y) and not (x < self._x and y > self._y):
                return

        values = [0] * 11

        values[0] = (x - self._coords[0][0]) * 24 - (y - self._coords[0][1]) * 24
        values[1] = (x - self._coords[0][0]) * 12 + (y - self._coords[0][1]) * 12

        values[0] += obj.align
        values[1] += self._square_height_top(self._x, self._y)

        if self._mmirror:
            values[0] += self._mmirror.align

        if obj.layer in (LAYER_LIVING, LAYER_EFFECT, LAYER_ITEM, LAYER_ITEM2):
            values[1] -= self._square_height_top(x, y)
        else:
            values[1] -= self._square_height(x, y, LAYER_FLOOR, obj.sub_layer)

        if obj.layer > LAYER_FLOOR:
            values[1] -= self._square_height(x, y, obj.layer, obj.sub_layer)

        values[2] = 4
        values[3] = 1

        if darkness > 640:
            values[4] = 0
        elif darkness > 320:
            values[4] = 1
        elif darkness > 160:
            values[4] = 2
        elif darkness > 80:
            values[4] = 3
        elif darkness > 40:
            values[4] = 4
        elif darkness > 20:
            values[4] = 5
        else:
            values[4] = 6

        values[5] = quick_pos
        values[6] = obj.alpha

        if self._mmirror and self._mmirror.last_heal and self._mmirror.last_heal != 100 and self._mmirror.path_attuned & (1 << (obj.layer - 1)):
            values[7] = values[8] = self._mmirror.last_heal
        else:
            values[7] = obj.zoom_x
            values[8] = obj.zoom_y

        values[9] = obj.rotate

        if obj.layer in (LAYER_FLOOR, LAYER_FMASK):
            values[10] = self._calculate_stretch(x, y, obj.sub_layer)

        for i in range(2):
            num_values = len(values)

            for j in range(len(values)):
                if not values[len(values) - 1 - j]:
                    num_values -= 1
                else:
                    break

            self._ret.append("[img={} {}]".format(obj.face[0], " ".join(str(x) for x in values[:num_values])))

            if (obj.f_draw_double and (x < self._x or y < self._y)) or obj.f_draw_double_always:
                values[1] -= 22
            else:
                break

    def _get_objects(self, x, y, layer, sub_layer):
        self._mmirror = self._mmirrors.get((x, y))

        locations = [(self._map, x, y)]

        if self._mmirror:
            mirror_map = self._mmirror.map if not self._mmirror.slaying else ReadyMap(self._mmirror.map.GetPath(self._mmirror.slaying))
            mirror_x = self._mmirror.x if self._mmirror.hp == -1 else self._mmirror.hp
            mirror_y = self._mmirror.y if self._mmirror.sp == -1 else self._mmirror.sp
            locations.append((mirror_map, mirror_x, mirror_y))

        for (m, x, y) in locations:
            try:
                objs = m.GetLayer(x, y, layer, sub_layer)
            except AtrinikError:
                continue

            if objs:
                return objs

        return []

    def _get_object(self, x, y, layer, sub_layer):
        objs = self._get_objects(x, y, layer, sub_layer)

        if objs:
            return objs[0]

        return None

    def create(self):
        self._calculate_coords()
        self._calculate_mmirrors()
        self._hide_objects()
        self._ret = ["[x={}]".format((self._range - 1) * 48 // 2)]

        for (x, y) in self._coords:
            for layer in [LAYER_FLOOR, LAYER_FMASK]:
                self._render(x, y, self._get_object(x, y, layer, 0))

        for (x, y) in self._coords:
            for layer in range(LAYER_FLOOR, NUM_LAYERS + 1):
                for sub_layer in range(NUM_SUB_LAYERS):
                    if sub_layer == 0 and (layer == LAYER_FLOOR or layer == LAYER_FMASK):
                        continue

                    self._render(x, y, self._get_object(x, y, layer, sub_layer))

        return "".join(self._ret)
