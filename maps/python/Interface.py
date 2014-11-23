from Atrinik import SetReturnValue, Type
import Language
import re

class Interface:
    def __init__(self, activator, npc):
        self._msg = ""
        self._links = []
        self._text_input = None
        self._activator = activator
        self._npc = npc
        self._restore = False
        self._append_text = None
        self._icon = None
        self._anim = None

        if npc:
            self.set_anim(npc.animation[1], npc.anim_speed, npc.direction)
            self.set_title(npc.name)

    def add_msg(self, msg, color = None, newline = True, **keywds):
        if newline and self._msg:
            self._msg += "\n\n"

        if color:
            self._msg += "[c=#" + color + "]"

        self._msg += msg.format(activator = self._activator, npc = self._npc, self = self, **keywds)

        if color:
            self._msg += "[/c]"

    def add_msg_icon(self, icon, desc = "", fit = False):
        self._msg += "\n\n"
        self._msg += "[bar=#000000 52 52][border=#606060 52 52][x=1][y=1][icon="
        self._msg += icon
        self._msg += " 50 50"

        if fit:
            self._msg += " 1"

        self._msg += "][x=-1][y=-1]"
        self._msg += "[padding=60][hcenter=50]"
        self._msg += desc
        self._msg += "[/hcenter][/padding]"

    def add_msg_icon_object(self, obj):
        self.add_msg_icon(obj.face[0], obj.GetName())

    def _get_dest(self, dest, npc = None):
        prepend = ""

        if not dest.startswith("/"):
            if npc:
                prepend = "/talk 5 \"{}\" ".format(npc)
            elif self._npc.env:
                if self._npc.env == self._activator:
                    prepend = "/talk 2 {} ".format(self._npc.count)
                elif self._npc.env == self._activator.Controller().container:
                    prepend = "/talk 4 {} ".format(self._npc.count)
            elif not self._npc.type in (Type.PLAYER, Type.MONSTER):
                prepend = "/talk 3 {} ".format(self._npc.count)

        return prepend + dest

    def add_link(self, link, action = "", dest = "", npc = None):
        dest = self._get_dest(dest, npc)

        if dest or action:
            self._links.append("[a=" + action + ":" + dest + "]" + link + "[/a]")
        elif not link.startswith("[a"):
            self._links.append("[a]" + link + "[/a]")
        else:
            self._links.append(link)

    def set_icon(self, icon):
        self._icon = icon

    def set_anim(self, anim, anim_speed = 1, direction = 0):
        self._anim = anim
        self._anim_speed = anim_speed
        self._direction = direction

    def set_title(self, title):
        self._title = title

    def set_text_input(self, text = "", prepend = "", allow_tab = False, allow_empty = False, cleanup_text = True, scroll_bottom = False, autocomplete = None):
        self._text_input = text
        self._text_input_prepend = self._get_dest(prepend)
        self._allow_tab = allow_tab
        self._allow_empty = allow_empty
        self._cleanup_text = cleanup_text
        self._scroll_bottom = scroll_bottom
        self._autocomplete = autocomplete

    def set_append_text(self, append_text):
        self._append_text = append_text

    def restore(self):
        self._restore = True

    def add_objects(self, objs):
        if type(objs) != list:
            objs = [objs]

        for obj in objs:
            # If the object is somewhere already, clone it.
            if obj.env or obj.map:
                obj = obj.Clone()

            self.add_msg_icon_object(obj)
            obj.InsertInto(self._activator)

    def dialog_close(self):
        self._activator.Controller().SendPacket(26, "", None)

    def finish(self, disable_timeout = False):
        if not self._msg and not self._restore:
            return

        pl = self._activator.Controller()

        if self._npc:
            SetReturnValue(-1 if disable_timeout or self._restore else len(self._msg))

        if not self._restore:
            # Construct the base data packet; contains the interface message,
            # the icon and the title.
            fmt = "BsBs"
            data = [0, self._msg, 3, self._title]

            if self._icon != None:
                fmt += "Bs"
                data += [2, self._icon]
            elif self._anim != None:
                fmt += "BHBB"
                data += [13, self._anim, self._anim_speed, self._direction]
        else:
            fmt = "B"
            data = [11]

        if self._append_text:
            fmt += "Bs"
            data += [12, self._append_text]

        # Add links to the data packet, if any.
        for link in self._links:
            fmt += "Bs"
            data += [1, link]

        # Add the text input, if any.
        if self._text_input != None:
            fmt += "Bs"
            data += [4, self._text_input]

            if self._text_input_prepend:
                fmt += "Bs"
                data += [5, self._text_input_prepend]

            if self._allow_tab:
                fmt += "B"
                data += [6]

            if not self._cleanup_text:
                fmt += "B"
                data += [7]

            if self._allow_empty:
                fmt += "B"
                data += [8]

            if self._scroll_bottom:
                fmt += "B"
                data += [9]

            if self._autocomplete:
                fmt += "Bs"
                data += [10, self._autocomplete]

        # Send the data.
        pl.SendPacket(26, fmt, *data)

class InterfaceBuilder(Interface):
    qm = None
    part = None
    matchers = []
    preconds = None

    def _part_dialog(self, part, checks):
        for check in checks:
            name = "_".join((self.dialog, check, part[-1:][0]))

            if not name in self.locals:
                continue

            if getattr(self.qm, check)(part):
                self.dialog = name
                self.part = part
                return True

        return False

    def _check_parts(self, parts, name = []):
        for part in parts:
            l = name + [part]

            if self._part_dialog(l, checks = ["need_start", "need_finish"]):
                return True

            if self.qm.started(l) and "parts" in parts[part]:
                if self._check_parts(parts[part]["parts"], l):
                    return True

            if self._part_dialog(l, checks = ["need_complete"]):
                return True

        return False

    def set_quest(self, qm):
        self.qm = qm

    def precond(self):
        return True

    def dialog(self, msg):
        pass

    @property
    def num2finish(self):
        return self.qm.num2finish(self.part)

    @property
    def numtofinish(self):
        return Language.int2english(self.num2finish())

    def finish(self, d, msg):
        self.locals = d
        self.dialog = "InterfaceDialog"
        dialog = None

        # Do some quest handling.
        if self.qm:
            # Quest is completed, regardless of parts, so show completed dialog.
            if self.qm.completed():
                dialog = "completed"
            else:
                self._check_parts(self.qm.quest["parts"])

        if dialog and self.dialog + "_" + dialog in self.locals:
            self.dialog += "_" + dialog

        if self.preconds:
            self.preconds(self)

        c = self.locals[self.dialog](self._activator, self._npc)
        c.set_quest(self.qm)
        c.part = self.part

        if not c.precond():
            Interface.finish(c)
            return

        for expr, callback in c.matchers:
            if re.match(expr, msg, re.I):
                callback(c)
                Interface.finish(c)
                return

        fnc = getattr(c, "dialog_" + msg.lower().replace(" ", "_"), None)

        if fnc == None:
            c.dialog(msg)
        else:
            fnc()

        Interface.finish(c)
