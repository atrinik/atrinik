"""
Handles Interface building and management.
"""

import re

import Atrinik
import Language


class Interface:
    """
    Used to create a single interface dialog -- essentially the interface
    window that appears in the client.
    """

    def __init__(self, activator, npc):
        """
        Initialize the class.

        :param activator: Player that activated the event.
        :type activator: :class:`Atrinik.Object.Object`
        :param npc: NPC/item that is speaking to the player.
        :type npc: :class:`Atrinik.Object.Object` or None
        """

        self._msg = ""
        self._title = ""
        self._links = []
        self._text_input = None
        self._activator = activator
        self._npc = npc
        self._restore = False
        self._append_text = None
        self._icon = None
        self._anim = None
        self._anim_speed = 0
        self._direction = 0
        self._objects = []
        self._text_input_prepend = None
        self._allow_tab = None
        self._allow_empty = None
        self._cleanup_text = None
        self._scroll_bottom = None
        self._autocomplete = None

        if npc:
            self.set_anim(npc.animation[1], npc.anim_speed, npc.direction)
            self.set_title(npc.name)

    def add_msg(self, msg, color=None, newline=True, **keywds):
        """
        Adds a message to the interface dialog.

        :param msg: Message to add.
        :type msg: str
        :param color: Color to use for the message, eg, 'ff0000' for red.
        :type color: str
        :param newline: If True, will add a blank line before the message (if
                        any was added previously).
        :type newline: bool
        :param \**keywds: Rest of the keyword arguments will be formatted into
                          the message using msg.format().
        """

        if newline and self._msg:
            self._msg += "\n\n"

        if color:
            self._msg += "[c=#" + color + "]"

        self._msg += msg.format(activator=self._activator, npc=self._npc,
                                self=self, **keywds)

        if color:
            self._msg += "[/c]"

    def add_msg_icon(self, icon, desc="", fit=False):
        """
        Adds an icon to the interface dialog.

        :param icon: In-game face name, eg, *amulet.101*.
        :type icon: str
        :param desc: Text to add alongside the icon.
        :type desc: str
        :param fit: If True, will fit the icon into the icon's box.
        :type fit: bool
        """

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

    def add_msg_icon_object(self, obj, desc=None):
        """
        Adds an object to the interface dialog as an icon.

        :param obj: Object to add.
        :type obj: :class:`Atrinik.Object.Object`
        :param desc: Text to add alongside the object's icon. If None, will use
                     the object's name.
        :type desc: str or None
        """

        pl = self._activator.Controller()
        if pl.s_socket_version < 1063:
            self.add_msg_icon(obj.face[0], desc or obj.GetName())
            return

        packet = obj.GetPacket(pl, Atrinik.UPD_ANIM | Atrinik.UPD_ANIMSPEED |
                               Atrinik.UPD_FACE | Atrinik.UPD_NROF |
                               Atrinik.UPD_GLOW)
        self._objects.append(packet)
        self._msg += "\n\n"
        self._msg += "[bar=#000000 52 52][border=#606060 52 52][x=1][y=1]"
        self._msg += "[obj={} 50 50]".format(obj.count)
        self._msg += "[x=-1][y=-1]"
        self._msg += "[padding=60][hcenter=50]"
        self._msg += desc or obj.GetName()
        self._msg += "[/hcenter][/padding]"

    def _get_dest(self, dest, npc=None):
        """
        Create a command from a dialog destination. This is necessary, because
        sometimes special commands are necessary for the interface links, for
        instance, when talking to an item and not an actual NPC.

        :param dest: Destination, eg, 'hello'. If this starts with a forward
        slash, nothing is done.
        :type dest: str
        :param npc: Specific NPC name to talk to.
        :type npc: str or None
        :return: The destination (or a command).
        """

        if dest.startswith("/"):
            return dest

        prepend = ""

        # Name of a specific NPC to talk to.
        if npc:
            prepend = "/talk 5 \"{}\" ".format(npc)
        # The NPC is inside an inventory.
        elif self._npc.env:
            # Inside the activator.
            if self._npc.env == self._activator:
                prepend = "/talk 2 {} ".format(self._npc.count)
            # Inside the activator's container.
            elif self._npc.env == self._activator.Controller().container:
                prepend = "/talk 4 {} ".format(self._npc.count)
        # The NPC is not alive (eg, a talking item).
        elif self._npc.type not in (Atrinik.Type.PLAYER, Atrinik.Type.MONSTER):
            prepend = "/talk 3 {} ".format(self._npc.count)

        return prepend + dest

    def add_link(self, link, action="", dest="", npc=None):
        """
        Adds a link to the interface dialog. Can be used interchangeably with
        :meth:`~Interface.add_msg`, and the links will always appear at the
        bottom of the interface, in the order that this method was called in.

        Essentially, this::

            inf = Interface(WhoIsActivator(), WhoAmI())
            inf.add_msg("Hello there!")
            inf.add_msg("Welcome to my inn.")
            inf.add_link("Who are you?", dest="who")

        Is the same as::

            inf = Interface(WhoIsActivator(), WhoAmI())
            inf.add_msg("Hello there!")
            inf.add_link("Who are you?", dest="who")
            inf.add_msg("Welcome to my inn.")

        However, it's still preferred to use the former form.

        :param link: Text of the link -- what the player can say to the NPC, eg,
                     'Who are you?'.
        :type link: str
        :param action: Special link action to perform.
        :type action: str
        :param dest: Destination dialog, eg, 'hello'.
        :type dest: str
        :param npc: Specific NPC name to talk to.
        :type npc: str or None
        """

        dest = self._get_dest(dest, npc)

        if dest or action:
            link = "[a={}:{}]{}[/a]".format(action, dest, link)
        elif not link.startswith("[a"):
            link = "[a]{}[/a]".format(link)

        self._links.append(link)

    def set_icon(self, icon):
        """
        Sets icon used for the interface dialog -- the picture in the upper
        left corner of the interface window.

        :param icon: In-game face name, eg, *amulet.101*.
        :type icon: str
        """

        self._icon = icon

    def set_anim(self, anim, anim_speed=1, direction=0):
        """
        Sets animation used for the interface dialog -- the picture in the upper
        left corner of the interface window.

        :param anim: Animation ID, eg, 111.
        :type anim: int
        :param anim_speed: Speed of the animation.
        :type anim_speed: int
        :param direction: Facing direction of the animation.
        :type direction: int
        """

        self._anim = anim
        self._anim_speed = anim_speed
        self._direction = direction

    def set_title(self, title):
        """
        Set title of the interface dialog.

        :param title: Title.
        :type title: str
        """

        self._title = title

    def set_text_input(self, text="", prepend="", allow_tab=False,
                       allow_empty=False, cleanup_text=True,
                       scroll_bottom=False, autocomplete=None):
        """
        Sets text input parameters. A text input box will be automatically
        opened in the interface.

        :param text: Text that will appear in the box.
        :type text: str
        :param prepend: Hidden text to prepend to the text typed into the input
                        box, before sending it as a talk command.
        :type prepend: str
        :param allow_tab: Whether to allow entering the tabulator key.
        :type allow_tab: bool
        :param allow_empty: Whether to allow sending empty text.
        :type allow_empty: bool
        :param cleanup_text: Whether to clean up the text before sending it, eg,
                             escape markup.
        :type cleanup_text: bool
        :param scroll_bottom: If True, will scroll to the bottom of the
                              interface dialog.
        :type scroll_bottom: bool
        :param autocomplete: Destination to use for auto-completion purposes.
        :type autocomplete: str or None
        """

        self._text_input = text
        self._text_input_prepend = self._get_dest(prepend)
        self._allow_tab = allow_tab
        self._allow_empty = allow_empty
        self._cleanup_text = cleanup_text
        self._scroll_bottom = scroll_bottom
        self._autocomplete = autocomplete

    def set_append_text(self, append_text):
        """
        Specifies text to append to an already visible interface dialog.

        :param append_text: Text to append.
        :type append_text: str
        """

        self._append_text = append_text

    def restore(self):
        """
        Restore previously shown interface dialog, and all the data associated
        with it.
        """

        self._restore = True

    def add_objects(self, objs):
        """
        Add objects to the interface dialog, as well as into the player's
        inventory. This is used to hand out rewards, for example.

        :param objs: Objects to add. If any of the objects are on a map or in an
                     inventory, they will be cloned.
        :type objs: :class:`Atrinik.Object.Object` or list of
                    :class:`Atrinik.Object.Object`
        """

        if type(objs) != list:
            objs = [objs]

        for obj in objs:
            # If the object is somewhere already, clone it.
            if obj.env or obj.map:
                obj = obj.Clone()

            obj.f_no_drop = obj.arch.clone.f_no_drop
            self.add_msg_icon_object(obj)
            obj.InsertInto(self._activator)

    def dialog_close(self):
        """
        Closes any previously opened interface dialog.
        """

        self._activator.Controller().SendPacket(26, "", None)

    def send(self):
        """
        Sends the interface dialog to the player's client.
        """

        if not self._msg and not self._restore:
            return

        pl = self._activator.Controller()

        if self._npc:
            Atrinik.SetReturnValue(-1 if self._restore else len(self._msg))

        if not self._restore:
            # Construct the base data packet; contains the interface message,
            # the icon and the title.
            fmt = "BsBs"
            data = [0, self._msg, 3, self._title]

            if self._icon is not None:
                fmt += "Bs"
                data += [2, self._icon]
            elif self._anim is not None:
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
        if self._text_input is not None:
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

        for obj in self._objects:
            obj_fmt, obj_data = obj
            fmt += "B" + obj_fmt
            data += [14] + obj_data

        # Send the data.
        pl.SendPacket(26, fmt, *data)


IB_CHECKS_STATE1 = frozenset(["need_complete_before_start", "need_start",
                              "need_finish"])
IB_CHECKS_STATE2 = frozenset(["need_complete"])


class InterfaceBuilder(Interface):
    """
    Implements the interface builder class, which is a more complex way of
    building interface dialogs based on preconditions, and handling specific
    response destinations through class methods.

    For example::

        from Interface import InterfaceBuilder

        class InterfaceDialog(InterfaceBuilder):
            def dialog_hello(self):
                self.add_msg("Hello there!")
                self.add_link("Who are you?", dest="who")

            def dialog_who(self):
                self.add_msg("I'm an NPC!")

        ib = InterfaceDialog(activator, me)
        ib.finish(locals(), msg)

    Using QuestManager automatic preconditions (note that this way is not
    generally used except for Python scripts generated from interface XML
    files)::

        from Interface import InterfaceBuilder
        from QuestManager import QuestManager
        from collections import OrderedDict

        example_quest = {
            'parts': OrderedDict((
                ('part1', {
                    'info': 'Kill the bear.',
                    'kill': {'nrof': 1},
                    'uid': 'part1',
                    'name': 'First Part',
                }),
            )),
            'name': "Example Quest",
            'uid': 'example_quest',
        }

        class InterfaceDialog_completed(InterfaceBuilder):
            def dialog_hello(self):
                self.add_msg("Thank you for helping me out earlier!")

        class InterfaceDialog_need_start_part1(InterfaceBuilder):
            def dialog_hello(self):
                self.add_msg("Welcome, I need your help killing a bear!")
                self.add_link("I'll help.", dest="dohelp")
            def dialog_dohelp(self):
                self.add_msg("Thanks! Go kill that bear then.")
                self.qm.start("part1")

        class InterfaceDialog_need_finish_part1(InterfaceBuilder):
            def dialog_hello(self):
                self.add_msg("Have you killed the bear yet?")

        class InterfaceDialog_need_complete_part1(InterfaceBuilder):
            def dialog_hello(self):
                self.add_msg("Amazing, thank you!")
                self.qm.complete("part1")

        ib = InterfaceBuilder(activator, me)
        ib.set_quest(QuestManager(activator, example_quest))
        ib.finish(locals(), msg)

    Using a preconditions function and regex dialog matchers (again, generally
    only used in generated Python scripts)::

        from Interface import InterfaceBuilder

        class InterfaceDialog_1(InterfaceBuilder):
            def dialog_hello(self):
                self.add_msg('Hello there!')
            def regex_dialog_abc(self):
                self.add_msg('ABC')
            matchers = [(r'a|b|c', regex_dialog_abc)]

        class InterfaceDialog_2(InterfaceBuilder):
            def dialog_hello(self):
                self.add_msg('Hello there 2!')
            def regex_dialog_def(self):
                self.add_msg('DEF')
            matchers = [(r'd|e|f', regex_dialog_def)]

        ib = InterfaceBuilder(activator, me)

        def preconds(self):
            if GetOptions() == 'interface1':
                self.dialog_name = 'InterfaceDialog_1'
            elif GetOptions() == 'interface2':
                self.dialog_name = 'InterfaceDialog_2'

        ib.preconds = preconds
        ib.finish(locals(), msg)
    """

    matchers = []

    def __init__(self, *args, **kwargs):
        """
        Initialize the InterfaceBuilder class.
        """

        super().__init__(*args, **kwargs)

        self.qm = None
        self.part = None
        self.dialog_name = None
        self.preconds = None
        self.locals = None

    def _part_dialog(self, part, checks):
        for check in checks:
            name = "_".join((self.dialog_name, check, part[-1:][0]))

            if name not in self.locals:
                continue

            if getattr(self.qm, check)(part):
                self.dialog_name = name
                self.part = part
                return True

        return False

    def _check_parts(self, parts, name=None):
        for part in parts:
            if name is not None:
                l = name + [part]
            else:
                l = [part]

            if self._part_dialog(l, checks=IB_CHECKS_STATE1):
                return True

            if self.qm.started(l) and "parts" in parts[part]:
                if self._check_parts(parts[part]["parts"], l):
                    return True

            if self._part_dialog(l, checks=IB_CHECKS_STATE2):
                return True

        return False

    def set_quest(self, qm):
        """
        Specifies a :class:`~QuestManager.QuestManager` instance to use for
        quest-based preconditions handling.

        :param qm: The quest manager instance.
        :type qm: :class:`QuestManager.QuestManager`
        """

        self.qm = qm

    @staticmethod
    def precond():
        """
        Precondition handler. Must return True, otherwise no dialog_xxx methods
        will be called.

        :return: True.
        :rtype: bool
        """

        return True

    def dialog(self, msg):
        """
        Performs handling of the specified message. Used when there's no
        dialog_xxx method implemented in the class.

        :param msg: Message to handle, eg, 'hello'.
        :type msg: str
        """

        pass

    @property
    def num2finish(self):
        """
        Acquire the number of items/kills/etc required to finish the quest
        part.

        :return: Number to finish.
        :rtype: int
        """

        return self.qm.num2finish(self.part)

    @property
    def numtofinish(self):
        """
        Acquire the number of items/kills/etc required to finish the quest
        part as a word, eg, "twenty five".

        :return: Number to finish.
        :rtype: str
        """

        return Language.int2english(self.num2finish)

    @property
    def gender(self):
        """
        Acquire the activator's gender as a string, using
        :attr:`Language.genders`.

        :return: The gender.
        :rtype: str
        """

        return Language.genders[self._activator.GetGender()]

    @property
    def gender2(self):
        """
        Acquire the activator's gender as a string, using
        :attr:`Language.genders2`.

        :return: The gender.
        :rtype: str
        """

        return Language.genders2[self._activator.GetGender()]

    def finish(self, locals_dict, msg, dialog_name="InterfaceDialog"):
        """
        Decides which interface dialog to use and sends it off.

        :param locals_dict: The locals() dictionary.
        :type locals_dict: dict
        :param msg: Message that was spoken to the NPC.
        :type msg: str
        :param dialog_name: Prefix of class names to look for, eg, only the
                            ones beginning with 'InterfaceDialog'.
        :type dialog_name: str
        """

        self.locals = locals_dict
        self.dialog_name = dialog_name
        dialog = None

        # Do some quest handling.
        if self.qm is not None:
            # Quest is completed, regardless of parts, so show completed dialog.
            if self.qm.completed():
                dialog = "completed"
            elif self.qm.failed():
                dialog = "failed"
            else:
                self._check_parts(self.qm.quest["parts"])

        if dialog and self.dialog_name + "_" + dialog in self.locals:
            self.dialog_name += "_" + dialog

        if self.preconds is not None:
            # noinspection PyCallingNonCallable
            self.preconds(self)

        c = self.locals[self.dialog_name](self._activator, self._npc)
        c.set_quest(self.qm)
        c.part = self.part

        if not c.precond():
            Interface.send(c)
            return

        for expr, callback in c.matchers:
            if re.match(expr, msg, re.I):
                callback(c)
                Interface.send(c)
                return

        fnc = getattr(c, "dialog_" + msg.lower().replace(" ", "_"), None)
        if fnc is None:
            c.dialog(msg)
        else:
            fnc()

        Interface.send(c)
