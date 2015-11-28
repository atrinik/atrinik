__author__ = "Alex Tokar"
__copyright__ = "Copyright (c) 2009-2015 Atrinik Development Team"
__credits__ = ["Alex Tokar"]
__license__ = "GPL"
__version__ = "2.0"
__maintainer__ = "Alex Tokar"
__email__ = "admin@atokar.net"

from xml.etree import ElementTree
from collections import OrderedDict
import os.path
import re

import utils
from compilers import BaseCompiler


try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO


class ParseError(SyntaxError):
    def __init__(self, msg, elem):
        SyntaxError.__init__(self, msg)
        self.elem = elem


class InterfaceIO(StringIO):
    def __init__(self, raw=False):
        StringIO.__init__(self)
        self.indent_level = 0
        self.raw = raw

    def write(self, s, raw=False, **kwargs):
        s = s.format(**kwargs)

        for line in s.split("\n"):
            if not self.raw:
                line = line.rstrip()

            StringIO.write(self, "{}{}{}".format(
                " " * (self.indent_level * 4) if not raw else "", line,
                "\n" if not self.raw else ""
            ))

    def indent(self, level=1):
        assert level >= 1
        self.indent_level += level

    def unindent(self, level=1):
        assert self.indent_level >= level
        self.indent_level -= level


class NPC(object):
    def __init__(self):
        self.head = InterfaceIO()
        self.body = InterfaceIO()
        self.tail = InterfaceIO()
        self.imports = ["Interface.InterfaceBuilder", "Atrinik.*"]
        self.preconds = OrderedDict()
        self.quest_uid = None

        self.tail.write("ib = InterfaceBuilder(activator, me)")

    def finish(self):
        for imp in self.imports:
            self.head.write("from {} import {}".format(*imp.split(".")))

        if self.quest_uid is not None:
            self.tail.write("ib.set_quest(QuestManager(activator, {uid}))",
                            uid=self.quest_uid)

        if self.preconds:
            self.tail.write("\n\ndef preconds(self):")
            self.tail.indent()

            self.tail.write("# noinspection PyRedundantParentheses")

            for i, uid in enumerate(self.preconds):
                self.tail.write("{statement} {precond}: ",
                                statement="if" if i == 0 else "elif",
                                precond=self.preconds[uid])
                self.tail.indent()
                self.tail.write("self.dialog_name = {dialog}",
                                dialog=repr("InterfaceDialog" + uid))
                self.tail.unindent()

            self.tail.unindent()
            self.tail.write("ib.preconds = preconds")

        self.tail.write("ib.finish(locals(), msg)")


class BaseTagCompiler(object):
    def __init__(self, compiler, parent=None):
        self.compiler = compiler
        self.handlers = {}
        self.parent = parent if parent is not None else self
        self.register_handlers()
        self.last_tag = None

    def register_handlers(self):
        pass

    def precompile(self, elem):
        pass

    def compile(self, elem):
        for node in elem:
            try:
                handler = self.handlers[node.tag](self.compiler, self)
            except KeyError:
                raise ParseError("Unknown tag", node)

            handler.precompile(elem=node)
            handler.compile(elem=node)
            handler.postcompile(elem=node)
            self.last_tag = handler

        self.finish(elem)

    def postcompile(self, elem):
        pass

    @property
    def npc(self):
        assert self.compiler.npc is not None
        return self.compiler.npcs[self.compiler.npc]

    def finish(self, tag):
        pass


class TagCompiler(BaseTagCompiler):
    def register_handlers(self):
        self.handlers["quest"] = TagCompilerQuest
        self.handlers["interface"] = TagCompilerInterface


class TagCompilerQuest(BaseTagCompiler):
    def __init__(self, *args):
        BaseTagCompiler.__init__(self, *args)
        self.data = {}

    def register_handlers(self):
        self.handlers["part"] = TagCompilerPart
        self.handlers["interface"] = TagCompilerInterface

    def compile(self, elem):
        self.data["name"] = elem.get("name")
        self.data["uid"] = os.path.basename(os.path.dirname(self.compiler.path))

        for attr in ["repeat", "repeat_delay"]:
            val = elem.get(attr)

            if val:
                self.data[attr] = int(val)

        BaseTagCompiler.compile(self, elem)

        self.compiler.quests.write("{uid} = {d}", uid=self.data["uid"],
                                   d=utils.dump_dict(self.data))


class TagCompilerPart(BaseTagCompiler):
    def __init__(self, *args):
        BaseTagCompiler.__init__(self, *args)
        self.data = {}

    def register_handlers(self):
        self.handlers["info"] = TagCompilerInfo
        self.handlers["item"] = TagCompilerItem
        self.handlers["kill"] = TagCompilerKill
        self.handlers["interface"] = TagCompilerInterface
        self.handlers["part"] = TagCompilerPart

    def compile(self, elem):
        if "parts" not in self.parent.data:
            self.parent.data["parts"] = OrderedDict()

        self.data["uid"] = re.sub(r"\W+", "", elem.get("uid"))
        self.data["name"] = elem.get("name", self.data["uid"])
        self.parent.data["parts"][self.data["uid"]] = self.data

        BaseTagCompiler.compile(self, elem)


class TagCompilerInfo(BaseTagCompiler):
    def compile(self, elem):
        self.parent.data["info"] = elem.text


class TagCompilerItem(BaseTagCompiler):
    def compile(self, elem):
        self.parent.data["item"] = {}
        self.parent.data["item"]["arch"] = elem.get("arch")
        self.parent.data["item"]["name"] = elem.get("name")

        for attr in ("nrof", "keep"):
            val = elem.get(attr)

            if val:
                self.parent.data["item"][attr] = int(val)


class TagCompilerKill(BaseTagCompiler):
    def compile(self, elem):
        self.parent.data["kill"] = {}
        nrof = elem.get("nrof")

        if nrof:
            self.parent.data["kill"]["nrof"] = int(nrof)


class TagCompilerInterface(BaseTagCompiler):
    def __init__(self, *args):
        BaseTagCompiler.__init__(self, *args)
        self.inherit = None
        self.interface_inherit = None
        self.regex_matchers = []
        self.uid = None
        self.old_pos = None

    def register_handlers(self):
        self.handlers["dialog"] = TagCompilerDialog
        self.handlers["and"] = TagCompilerAnd
        self.handlers["code"] = TagCompilerCode
        self.handlers["precond"] = TagCompilerPrecond

    def compile(self, elem):
        self.compiler.npc = elem.get("npc")

        if self.compiler.npc:
            self.compiler.npc = re.sub(
                r"\W+", "", self.compiler.npc.lower().replace(" ", "_")
            )
        else:
            self.compiler.npc = os.path.basename(self.compiler.path)[:-4]

        if self.compiler.npc not in self.compiler.npcs:
            self.compiler.npcs[self.compiler.npc] = NPC()

        if isinstance(self.parent, (TagCompilerQuest, TagCompilerPart)):
            self.npc.quest_uid = os.path.basename(
                os.path.dirname(self.compiler.path)
            )

            for imp in ("QuestManager.QuestManager",
                        "InterfaceQuests." + self.npc.quest_uid):
                if imp not in self.npc.imports:
                    self.npc.imports.append(imp)

        state = elem.get("state")
        self.inherit = []
        self.interface_inherit = []
        for val in elem.get("inherit", "").split(","):
            val = val.strip()
            if val:
                self.inherit.append(val)

        for val in self.inherit:
            if val.find(".") != -1:
                if val not in self.npc.imports:
                    self.npc.imports.append(val)

                val = val[val.find(".") + 1:]
            elif val == "interface":
                val = "InterfaceDialog"
            else:
                val = "InterfaceDialog_" + val

            self.interface_inherit.append(val)

        if not self.interface_inherit:
            self.interface_inherit.append("InterfaceBuilder")

        if not state:
            self.uid = ""
        else:
            self.uid = "_" + state

            if isinstance(self.parent, TagCompilerPart):
                self.uid += "_" + self.parent.data["uid"]

        self.npc.body.write("\n")
        self.npc.body.write("# noinspection PyPep8Naming")
        self.npc.body.write(
            "class InterfaceDialog{uid}({inherit}):", uid=self.uid,
            inherit=", ".join(self.interface_inherit)
        )

        self.npc.body.indent()

        self.old_pos = self.npc.body.tell()
        BaseTagCompiler.compile(self, elem)

    def finish(self, tag):
        if self.npc.body.tell() == self.old_pos:
            self.npc.body.write("pass")

        matchers = ",".join("(r{expr}, {dest})".format(
            expr=repr(expr), dest=dest) for expr, dest in self.regex_matchers
        )

        if matchers:
            self.npc.body.write("matchers = [{matchers}]", matchers=matchers)

        self.npc.body.unindent()


class TagCompilerDialog(BaseTagCompiler):
    def __init__(self, *args):
        BaseTagCompiler.__init__(self, *args)
        self.was_inherited = False
        self.inherit = None
        self.inherit2 = None
        self.inherit_name = None
        self.inherit_name_prefix = None
        self.inherit_args = None
        self.old_pos = None

    def register_handlers(self):
        self.handlers["and"] = TagCompilerAnd
        self.handlers["say"] = TagCompilerSay
        self.handlers["close"] = TagCompilerClose
        self.handlers["action"] = TagCompilerAction
        self.handlers["notification"] = TagCompilerNotification
        self.handlers["object"] = TagCompilerObject
        self.handlers["inherit"] = TagCompilerInherit
        self.handlers["choice"] = TagCompilerChoice
        self.handlers["message"] = TagCompilerMessage
        self.handlers["response"] = TagCompilerResponse

    def compile(self, elem):
        self.npc.closed = False

        dialog_name = elem.get("name", "")
        dialog_regex = elem.get("regex")
        dialog_prepend = ""

        if dialog_name:
            dialog_name = "_" + dialog_name.replace(" ", "_")
            dialog_args = ""
        else:
            dialog_args = ", msg"

        if not self.parent.inherit or dialog_name.startswith("_::"):
            self.inherit = "self"
        else:
            self.inherit = "super()"

        self.inherit_name = elem.get("inherit")
        self.inherit_name_prefix = ""

        if not self.inherit_name:
            self.inherit_name = dialog_name
        else:
            if self.inherit_name.startswith("::"):
                self.inherit_name = self.inherit_name[2:]
                self.inherit_name_prefix = "sub"

            self.inherit_name = "_" + self.inherit_name.replace(" ", "_")

        if dialog_name.startswith("_::"):
            dialog_prepend = "sub"
            dialog_name = "_" + dialog_name[3:]
        elif dialog_regex:
            dialog_prepend = "regex_"

            if dialog_name:
                dest = "{dialog_prepend}dialog{dialog_name}".format(
                    dialog_prepend=dialog_prepend, dialog_name=dialog_name
                )
            else:
                if not self.parent.inherit:
                    dest_inherit = ""
                else:
                    dest_inherit = self.parent.interface_inherit[0] + "."

                dest = "{dest_inherit}dialog{inherit_name}".format(
                    dest_inherit=dest_inherit, inherit_name=self.inherit_name
                )

            self.parent.regex_matchers.append((dialog_regex, dest))

        if not dialog_regex or dialog_name:
            self.npc.body.write(
                "\ndef {dialog_prepend}dialog{dialog_name}(self{dialog_args}):",
                dialog_prepend=dialog_prepend, dialog_name=dialog_name,
                dialog_args=dialog_args
            )

        self.npc.body.indent()

        if dialog_regex and not dialog_name:
            self.finish(elem)
            return

        title = elem.get("title")
        icon = elem.get("icon")
        animation = elem.get("animation")

        if title:
            self.npc.body.write("self.set_title({title})", title=repr(title))

        if icon == "player":
            self.npc.body.write(
                "self.set_icon(self._activator.arch.clone.face[0])"
            )
        elif icon:
            self.npc.body.write("self.set_icon({icon})", icon=repr(icon))

        if animation == "player":
            self.npc.body.write(
                "self.set_anim(self._activator.animation[1],"
                "self._activator.anim_speed, self._activator.direction)"
            )
        elif animation:
            self.npc.body.write("self.set_anim({animation})",
                                animation=repr(animation))

        self.old_pos = self.npc.body.tell()
        BaseTagCompiler.compile(self, elem)

    def do_inherit(self):
        self.was_inherited = True
        self.npc.body.write(
            "{inherit}.{inherit_name_prefix}dialog{inherit_name}()",
            inherit=self.inherit, inherit_name_prefix=self.inherit_name_prefix,
            inherit_name=self.inherit_name
        )

    def finish(self, tag):
        if tag.get("inherit") and tag.get("name") and not self.was_inherited:
            self.npc.body.seek(self.old_pos)
            data = self.npc.body.read().rstrip()
            self.npc.body.seek(self.old_pos)
            self.do_inherit()

            if data:
                self.npc.body.write("{data}", raw=True, data=data)

        self.npc.body.unindent()


class TagCompilerAnd(BaseTagCompiler):
    def __init__(self, *args):
        BaseTagCompiler.__init__(self, *args)
        self.precond = InterfaceIO(raw=True)

    def register_handlers(self):
        self.handlers["check"] = TagCompilerCheck
        self.handlers["ncheck"] = TagCompilerNcheck
        self.handlers["and"] = TagCompilerAnd
        self.handlers["or"] = TagCompilerOr

        if isinstance(self.parent, TagCompilerDialog):
            self.handlers["message"] = TagCompilerMessage
            self.handlers["inherit"] = TagCompilerInherit
            self.handlers["object"] = TagCompilerObject
            self.handlers["action"] = TagCompilerAction

    def precompile(self, elem):
        if not isinstance(self, TagCompilerAnd):
            self.precond.seek(0, 0)
            return

        if self.parent.last_tag is not None:
            if isinstance(self.parent, TagCompilerOr):
                self.precond.write(" or ")
            elif isinstance(self.parent, TagCompilerAnd):
                self.precond.write(" and ")

    def compile(self, elem):
        self.precond.write("(")
        self.npc.body.indent()
        old_pos = self.npc.body.tell()
        BaseTagCompiler.compile(self, elem)
        self.npc.body.unindent()
        self.precond.write(")")

        if isinstance(self.parent, TagCompilerInterface):
            self.npc.preconds[self.parent.uid] = self.precond.getvalue()
        elif isinstance(self.parent, TagCompilerDialog):
            self.npc.body.seek(old_pos)
            data = self.npc.body.read().rstrip()
            self.npc.body.seek(old_pos)

            precond = self.precond.getvalue()

            self.npc.body.write("# noinspection PyRedundantParentheses")

            if not data:
                self.npc.body.write("return {precond}", precond=precond)
            elif precond == "()":
                self.npc.body.write("else:")
            else:
                if type(self.parent.last_tag) is TagCompilerAnd:
                    statement = "elif"
                else:
                    statement = "if"

                self.npc.body.write("{statement} {precond}:",
                                    statement=statement, precond=precond)

            if data:
                self.npc.body.write("{data}", raw=True, data=data)

    def postcompile(self, elem):
        if isinstance(self.parent, (TagCompilerAnd, TagCompilerOr)):
            self.parent.precond.write(self.precond.getvalue())


class TagCompilerOr(TagCompilerAnd):
    pass


class TagCompilerCheck(TagCompilerAnd):
    def register_handlers(self):
        self.handlers["object"] = TagCompilerObject

    def compile(self, elem):
        not_str = "not " if isinstance(self, TagCompilerNcheck) else ""
        self.precond.write("{not_str}(", not_str=not_str)
        BaseTagCompiler.compile(self, elem)

        for attr in elem.attrib:
            val = elem.get(attr)

            if not val:
                continue

            if not self.precond.getvalue().endswith("("):
                self.precond.write(" and ")

            if attr == "region_map":
                self.precond.write(
                    "self._activator.FindObject(type=Type.REGION_MAP, "
                    "name={name})", name=repr(val)
                )
            elif attr == "options":
                self.precond.write("GetOptions() == {options}",
                                   options=repr(val))
            elif attr == "enemy":
                self.precond.write("self._npc.enemy")

                if val != "any":
                    self.precond.write(" == ")

                    if val == "player":
                        self.precond.write("self._activator")
            elif attr == "num2finish":
                self.precond.write("self.num2finish == {num}",
                                   num=repr(int(val)))
            elif attr in ("started", "finished", "completed", "failed"):
                words = elem.attrib[attr].split(" ")

                if len(words) == 2:
                    quest_name, part_name = words
                elif not self.npc.quest_uid:
                    quest_name = elem.attrib[attr]
                    part_name = ""
                else:
                    quest_name = self.npc.quest_uid
                    part_name = elem.attrib[attr]

                if part_name:
                    split = part_name.split("::")

                    if len(split) > 1:
                        part_name = "[{}]".format(
                            ", ".join(repr(val) for val in split)
                        )
                    else:
                        part_name = repr(part_name)

                if quest_name:
                    self.precond.write("self.qm.{attr}({part_name})",
                                       attr=attr, part_name=part_name)

                    for imp in ("QuestManager.QuestManager",
                                "InterfaceQuests." + quest_name):
                        if imp not in self.npc.imports:
                            self.npc.imports.append(imp)
                else:
                    self.precond.write(
                        "QuestManager(self._activator, {quest_name})"
                        ".{attr}({part_name})", quest_name=quest_name,
                        attr=attr, part_name=part_name)
            elif attr == "gender":
                self.precond.write("self._activator.GetGender() == ")

                if val == "male":
                    self.precond.write("Gender.MALE")
                elif val == "female":
                    self.precond.write("Gender.FEMALE")
                elif val == "hermaphrodite":
                    self.precond.write("Gender.HERMAPHRODITE")
                else:
                    self.precond.write("Gender.NEUTER")
            elif attr == "faction_friend":
                self.precond.write(
                    "self._activator.FactionIsFriend({faction})",
                   faction=repr(val))

        self.precond.write(")")


class TagCompilerNcheck(TagCompilerCheck):
    pass


class TagCompilerSay(BaseTagCompiler):
    def compile(self, elem):
        self.npc.body.write("self._npc.Say({text})", text=repr(elem.text))


class TagCompilerClose(BaseTagCompiler):
    def compile(self, elem):
        self.npc.body.write("self.dialog_close()")
        self.npc.closed = True


class TagCompilerAction(BaseTagCompiler):
    def compile(self, elem):
        if elem.text:
            self.npc.body.write("{text}", text=elem.text)

        for attr in ["start", "complete", "region_map", "enemy", "teleport",
                     "trigger", "cast", "fail"]:
            val = elem.get(attr)

            if not val:
                continue

            if attr == "region_map":
                self.npc.body.write(
                    "self._activator.CreateObject('region_map').name = {name}",
                    name=repr(val)
                )
            elif attr == "enemy":
                if val == "player":
                    enemy = "self._activator"
                elif val == "clear":
                    enemy = "None"
                else:
                    raise ParseError("Invalid data in tag", elem)

                self.npc.body.write("self._npc.WriteKey('was_provoked', '1')")
                self.npc.body.write("self._npc.enemy = {enemy}", enemy=enemy)
            elif attr == "teleport":
                match = re.match(r"([^ ]+)\s*(\d+)?\s*(\d+)?\s*(savebed)?", val)

                if not match:
                    continue

                args = []

                for i, val in enumerate(match.groups()):
                    if val:
                        args.append((repr(val) if i == 0 else val))

                    if i == 2:
                        break

                self.npc.body.write("self._activator.TeleportTo({args})",
                                    args=", ".join(args))

                if match.group(4):
                    attrs = ["savebed_map", "bed_x", "bed_y"]

                    for i, val in enumerate(args):
                        self.npc.body.write(
                            "self._activator.Controller().{attr} = "
                            "{val}".format(attr=attrs[i], val=val))
            elif attr == "trigger":
                self.npc.body.write(
                    "beacon = self._npc.map.LocateBeacon({name})\n"
                    "beacon.env.Apply(beacon.env, APPLY_NORMAL)",
                    name=repr(val)
                )
            elif attr == "cast":
                self.npc.body.write("self._npc.Cast(GetArchetype({spell})."
                                    "clone.sp, self._activator)",
                                    spell=repr("spell_" +
                                               val.replace(" ", "_")))
            else:
                split = val.split("::")

                if len(split) > 1:
                    val = "[{}]".format(", ".join(repr(val) for val in split))
                else:
                    val = repr(val)

                self.npc.body.write("self.qm.{attr}({val})", attr=attr, val=val)


class TagCompilerNotification(BaseTagCompiler):
    def compile(self, elem):
        if "Packet.Notification" not in self.npc.imports:
            self.npc.imports.append("Packet.Notification")

        self.npc.body.write(
            "Notification(self._activator.Controller(), {message}, {action}, "
            "{shortcut}, {delay})", message=repr(elem.get("message")),
            action=repr(elem.get("action", None)),
            shortcut=repr(elem.get("shortcut", None)),
            delay=repr(int(elem.get("delay", 0)))
        )


class TagCompilerObject(BaseTagCompiler):
    def compile(self, elem):
        item_args = []

        for attr, attr2 in [("arch", "archname"), ("name", "name")]:
            val = elem.get(attr)

            if val:
                item_args.append("{}={}".format(attr2, repr(val)))

        item_args = ", ".join(item_args)
        remove = elem.get("remove")
        message = elem.get("message")
        parent = self.parent

        if isinstance(parent, (TagCompilerCheck, TagCompilerNcheck)):
            parent.precond.write(
                "self._activator.FindObject({item_args})",
                item_args=item_args)
        elif not remove:
            nrof = elem.get("nrof") or 0
            if nrof:
                nrof = int(nrof)

            if isinstance(parent, TagCompilerMessage):
                self.npc.body.write(
                    "obj = self._npc.FindObject({item_args})",
                    item_args=item_args
                )
                self.npc.body.write(
                    "self.add_msg_icon_object(obj, desc={message})",
                    message=repr(message)
                )
                return

            if nrof > 0:
                self.npc.body.write(
                    "obj = CreateObject({arch})",
                    arch=repr(elem.get("arch"))
                )

                if nrof > 1:
                    self.npc.body.write(
                        "obj.nrof = {nrof}",
                        nrof=repr(nrof)
                    )


            if not self.npc.closed and not message:
                if not nrof:
                    self.npc.body.write(
                        "obj = self._npc.FindObject({item_args})",
                        item_args=item_args
                    )

                self.npc.body.write("self.add_objects(obj)")
            else:
                if not nrof:
                    self.npc.body.write(
                        "obj = self._npc.FindObject({item_args}).Clone()",
                        item_args=item_args
                    )

                if not self.npc.closed and message:
                    self.npc.body.write(
                        "self.add_msg_icon_object(obj, desc={message})",
                        message=repr(message)
                    )

                self.npc.body.write("obj.InsertInto(self._activator)")
        else:
            self.npc.body.write(
                "self._activator.FindObject({item_args}).Decrease({remove})",
                item_args=item_args, remove=remove if remove != "1" else ""
            )


class TagCompilerInherit(BaseTagCompiler):
    def compile(self, elem):
        name = elem.get("name")

        if name:
            if name.startswith("::"):
                dialog_prefix = "sub"
            else:
                dialog_prefix = ""

            name = "_" + name.replace("::", "")
            self.npc.body.write("self.{prefix}dialog{name}()",
                                prefix=dialog_prefix, name=name)
        else:
            self.parent.do_inherit()


class TagCompilerChoice(BaseTagCompiler):
    def register_handlers(self):
        self.handlers["message"] = TagCompilerMessage

    def compile(self, elem):
        if "random.choice" not in self.npc.imports:
            self.npc.imports.append("random.choice")

        msgs = ", ".join(repr(msg.text) for msg in elem.findall("message"))
        self.npc.body.write("self.add_msg(choice([{msgs}]))", msgs=msgs)


class TagCompilerMessage(BaseTagCompiler):
    def register_handlers(self):
        self.handlers["object"] = TagCompilerObject

    def compile(self, elem):
        color = elem.get("color", "")
        msg = repr(elem.text.strip() if elem.text else elem.text)

        if color:
            if color.startswith("#"):
                color = repr(color[1:])
            else:
                color = "COLOR_{}".format(color.upper())

            color = ", color={}".format(color)

        if not self.npc.closed:
            self.npc.body.write("self.add_msg({msg}{color})", msg=msg,
                                color=color)
            BaseTagCompiler.compile(self, elem)
        else:
            self.npc.body.write(
                "self._activator.Controller().DrawInfo({msg}{color})", msg=msg,
                color=color
            )


class TagCompilerResponse(BaseTagCompiler):
    def compile(self, elem):
        message = elem.get("message")
        link_args = ""

        for attr, attr2 in [("destination", "dest"), ("action", "action"),
                            ("npc", "npc")]:
            val = elem.get(attr)

            if val:
                link_args += ", {}={}".format(attr2, repr(val))

        self.npc.body.write("self.add_link({message}{link_args})",
                            message=repr(message), link_args=link_args)


class TagCompilerCode(BaseTagCompiler):
    def compile(self, elem):
        self.npc.body.write("{text}", text=elem.text)


class TagCompilerPrecond(BaseTagCompiler):
    def compile(self, elem):
        self.npc.body.write("def precond(self):")
        self.npc.body.indent()
        self.npc.body.write("{text}", text=elem.text)
        self.npc.body.unindent()


class InterfaceCompiler(BaseCompiler):
    def __init__(self, *args):
        BaseCompiler.__init__(self, *args)
        self.npc = None
        self.npcs = {}
        self.path = None
        self.quests = InterfaceIO()
        self.quests.write("from collections import OrderedDict")

    def compile(self):
        for path in utils.find_files(os.path.join(self.paths["maps"],
                                                  "interfaces"), ".xml"):
            self.path = path
            self.compile_file()

        with open(os.path.join(self.paths["maps"], "python",
                               "InterfaceQuests.py"), "wb") as quests_file:
            quests_file.write(self.quests.getvalue().encode())

    def compile_file(self):
        self.npcs.clear()
        self.npc = None

        try:
            tree = ElementTree.parse(self.path)
        except ElementTree.ParseError as e:
            print("Error parsing {}: {}".format(self.path, e))
            return

        root = tree.getroot()
        tag_compiler = TagCompiler(self)

        try:
            tag_compiler.compile(root)
        except ParseError as e:
            print("Error parsing {}: {}: <{}>".format(self.path, e, e.elem.tag))
            return

        for npc in self.npcs:
            self.npcs[npc].finish()

            with open(os.path.join(os.path.dirname(self.path),
                                   npc + ".py"), "wb+") as npc_file:
                npc_file.write(self.npcs[npc].head.getvalue().encode())
                npc_file.write(self.npcs[npc].body.getvalue().encode())
                npc_file.write(self.npcs[npc].tail.getvalue().encode())
