"""Generates documentation for the Atrinik Python package.

The documentation is generated in the form of Python files that mimic the
C interface. Only the API is exposed, there is no implementation.

Documentation is collected from (and also dynamically generated ) docstrings
found in Atrinik classes, methods, etc.

The generated documentation is in Restructured Text (reST) format, and can be
found in /maps/python/Atrinik.

IDEs that support the reST format (such as PyCharm) can use this generated
Python interface to add type-hinting to functions and methods, and to even
resolve the Atrinik package definitions.
"""

import sys
import inspect
import os
import re
from collections import OrderedDict

from Atrinik import *
from CParser import CParser

PATH = os.path.join(GetSettings()["mapspath"], "python", "Atrinik")


def getargspec(obj):
    if not obj.__doc__:
        return []

    match = re.search(r"[\w_]+\((.*)\)", obj.__doc__, re.M)
    if not match:
        print("Failed to get args for {}".format(obj))
        return []

    args = re.findall(r"([\w+_]+)(?:=([\w_\-\.]+))?", match.group(1))
    return ["=".join(x for x in val if x) for val in args]


def dump_docstring(obj, f, indent=0, obj_name=None, is_getter=False,
                   is_setter=False, doc=None):
    if doc is None:
        doc = obj.__doc__

    if not doc:
        return

    ret = None

    if is_getter or is_setter:
        parts = doc.split(";")
        if obj_name.startswith("f_"):
            parts.append("bool")
        if len(parts) < 2:
            print("Invalid object {}".format(obj))
            return
        types = re.match(r"([^\(]+)\s*(\(.*\))?", parts[1])
        if not types:
            print("No types for {}".format(obj))
            return

        tmp_type = types.group(1).strip()
        extra = types.group(2) or ""
        doc = parts[0].strip() + " " + extra.strip()

        ret = []
        types = []
        for val in tmp_type.split(" "):
            if val != "or":
                ret.append(val)
                types.append(":class:`{}`".format(val))

        if is_getter:
            doc += "\n\n:type: " + " or ".join(types)
        else:
            doc += "\n\n:param value: The value to set.\n"
            doc += ":type value: {}".format(" or ".join(ret))
    elif obj_name is not None:
        doc = ".. class:: {}\n\n".format(obj_name) + doc

    f.write(" " * indent * 4)
    f.write('"""\n')
    iterator = iter(doc.split("\n"))

    for line in iterator:
        if line.startswith(".. function::") or line.startswith(".. method::"):
            next(iterator)
            continue

        f.write(" " * indent * 4)
        f.write(line + "\n")

    f.write(" " * indent * 4)
    f.write('"""\n')

    return ret


def dump_obj(obj, f, indent=0, defaults=None):
    names = []
    l = dir(obj)
    if defaults is not None:
        l += list(defaults.keys())

    for tmp_name in l:
        if tmp_name.startswith("__") and tmp_name.endswith("__"):
            continue

        if tmp_name == "print":
            continue

        if hasattr(obj, tmp_name):
            tmp = getattr(obj, tmp_name)
            doc = None
        else:
            tmp = defaults[tmp_name][0]
            doc = defaults[tmp_name][1]

        if inspect.ismodule(tmp):
            f.write("import Atrinik.{name} as {name}\n".format(name=tmp_name))

            with open(os.path.join(PATH, tmp_name + ".py"), "w") as f2:
                dump_docstring(tmp, f2, indent)
                f2.write("import Atrinik\n")
                dump_obj(tmp, f2)
        elif inspect.isclass(tmp):
            f.write("import Atrinik.{name} as {name}\n".format(name=tmp_name))

            with open(os.path.join(PATH, tmp_name + ".py"), "w") as f2:
                f2.write("import Atrinik\n")
                f2.write("\n\n")
                f2.write(" " * indent * 4)
                f2.write("class {}(object):\n".format(tmp_name))
                dump_docstring(tmp, f2, indent + 1, obj_name=tmp_name)
                dump_obj(tmp, f2, indent=1)
        elif hasattr(tmp, "__call__"):
            args = getargspec(tmp)
            if inspect.isclass(obj):
                args.insert(0, "self")
            else:
                f.write("\n")

            f.write("\n")
            f.write(" " * indent * 4)
            f.write("def {}({}):\n".format(tmp_name, ", ".join(args)))
            dump_docstring(tmp, f, indent + 1)
            f.write(" " * (indent + 1) * 4)
            f.write("pass\n")
        elif isinstance(tmp, (Object, Map, Archetype, Player)):
            f.write(" " * indent * 4)
            f.write("{} = {cls_name}.{cls_name}()\n".format(
                tmp_name, cls_name=tmp.__class__.__name__))
            doc = tmp_name.replace("_", " ").title()
            dump_docstring(tmp, f, indent, doc=doc)
            continue
        elif inspect.isclass(obj):
            f.write("\n")
            f.write(" " * indent * 4)
            f.write("@property\n")
            f.write(" " * indent * 4)
            f.write("def {}(self):\n".format(tmp_name, tmp))
            types = dump_docstring(tmp, f, indent + 1, is_getter=True,
                                   obj_name=tmp_name)
            f.write(" " * (indent + 1) * 4)
            f.write("value = getattr(self, {})\n".format(repr(tmp_name)))

            if types is not None:
                f.write(" " * (indent + 1) * 4)
                f.write("assert isinstance(value, ({},))\n".format(
                    ", ".join(types)))

            f.write(" " * (indent + 1) * 4)
            f.write("return value\n\n")
            f.write(" " * indent * 4)
            f.write("@{}.setter\n".format(tmp_name))
            f.write(" " * indent * 4)
            f.write("def {}(self, value):\n".format(tmp_name, tmp))
            dump_docstring(tmp, f, indent + 1, is_setter=True,
                           obj_name=tmp_name)
            f.write(" " * (indent + 1) * 4)
            f.write("setattr(self, {}, value)\n".format(repr(tmp_name)))
        else:
            f.write(" " * indent * 4)
            f.write("{} = {}\n".format(tmp_name, repr(tmp)))

            parent_name = obj.__name__.replace("Atrinik_", "").upper()
            for x in (tmp_name, parent_name + "_" + tmp_name):
                if x in matches:
                    doc = matches[x]["comment"]
                    break

            if not doc:
                print("Undocumented constant: {}".format(tmp_name))
                doc = tmp_name.replace("_", " ").title()

            dump_docstring(tmp, f, indent, doc=doc)

        names.append(repr(tmp_name))

    return names


def main():
    defaults = OrderedDict([
        ("activator", (Object(), ":class:`~Atrinik.Object.Object` that "
                                 "activated the event.")),
        ("pl", (Player(), "If the event activator is a player, this will be a "
                          ":class:`~Atrinik.Player.Player` instance, otherwise "
                          "it will be None.")),
        ("me", (Object(), ":class:`~Atrinik.Object.Object` that has the event "
                          "object in its inventory that triggered the event.")),
        ("msg", ("hello", "Message used to activate the event (eg, in case of "
                         "say events). Can be None.")),
    ])

    if not os.path.exists(PATH):
        os.makedirs(PATH)

    with open(os.path.join(PATH, "__init__.py"), "w") as f:
        obj = sys.modules["Atrinik"]
        dump_docstring(obj, f)
        names = dump_obj(obj, f, defaults=defaults)
        f.write("__all__ = [{}]\n".format(", ".join(names)))


parser = CParser()
matches = {}

for root, dirs, files in os.walk("src"):
    for file in files:
        matches.update(parser.parse(os.path.join(root, file)))

main()
