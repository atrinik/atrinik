from Atrinik import SetReturnValue, Type

class Interface:
    def __init__(self, activator, npc):
        self._msg = ""
        self._links = []
        self._text_input = None
        self._activator = activator
        self._npc = npc
        self._restore = False
        self._append_text = None

        if npc:
            self._icon = npc.face[0][:-1] + "1"
            self._title = npc.name

    def add_msg(self, msg, color = None, newline = True):
        if newline and self._msg:
            self._msg += "\n\n"

        if color:
            self._msg += "<c=#" + color + ">"

        self._msg += msg

        if color:
            self._msg += "</c>"

    def add_msg_icon(self, icon, desc = "", fit = False):
        self._msg += "\n\n"
        self._msg += "<bar=#000000 52 52><border=#606060 52 52><x=1><y=1><icon="
        self._msg += icon
        self._msg += " 50 50"

        if fit:
            self._msg += " 1"

        self._msg += "><x=-1><y=-1>"
        self._msg += "<padding=60><hcenter=50>"
        self._msg += desc
        self._msg += "</hcenter></padding>"

    def add_msg_icon_object(self, obj):
        self.add_msg_icon(obj.face[0], obj.GetName())

    def _get_dest(self, dest):
        prepend = ""

        if not dest.startswith("/"):
            if self._npc.env:
                if self._npc.env == self._activator:
                    prepend = "/talk 2 {} ".format(self._npc.count)
                elif self._npc.env == self._activator.Controller().container:
                    prepend = "/talk 4 {} ".format(self._npc.count)
            elif not self._npc.type in (Type.PLAYER, Type.MONSTER):
                prepend = "/talk 3 {} ".format(self._npc.count)

        return prepend + dest

    def add_link(self, link, action = "", dest = ""):
        dest = self._get_dest(dest)

        if dest or action:
            self._links.append("<a=" + action + ":" + dest + ">" + link + "</a>")
        elif not link.startswith("<a"):
            self._links.append("<a>" + link + "</a>")
        else:
            self._links.append(link)

    def set_icon(self, icon):
        self._icon = icon

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
            SetReturnValue(1)

        if not self._restore:
            # Construct the base data packet; contains the interface message,
            # the icon and the title.
            fmt = "BsBsBs"
            data = [0, self._msg, 2, self._icon, 3, self._title]
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

        # If there is any movement behavior, update the amount of time
        # the NPC should pause moving for.
        if self._npc and not disable_timeout and (self._npc.move_type or self._npc.f_random_move):
            from Atrinik import GetTicks, INTERFACE_TIMEOUT_CHARS, INTERFACE_TIMEOUT_SECONDS, INTERFACE_TIMEOUT_INITIAL, MAX_TIME

            timeout = self._npc.ReadKey("npc_move_timeout")
            ticks = GetTicks() + ((int(max(INTERFACE_TIMEOUT_CHARS, len(self._msg)) / INTERFACE_TIMEOUT_CHARS * INTERFACE_TIMEOUT_SECONDS)) - INTERFACE_TIMEOUT_SECONDS + INTERFACE_TIMEOUT_INITIAL) * (1000000 // MAX_TIME)

            if not timeout or ticks > int(timeout):
                self._npc.WriteKey("npc_move_timeout", str(ticks))
