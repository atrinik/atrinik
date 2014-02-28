#!/usr/bin/python
#*************************************************************************
#*            Atrinik, a Multiplayer Online Role Playing Game            *
#*                                                                       *
#*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
#*                                                                       *
#* Fork from Crossfire (Multiplayer game for X-windows).                 *
#*                                                                       *
#* This program is free software; you can redistribute it and/or modify  *
#* it under the terms of the GNU General Public License as published by  *
#* the Free Software Foundation; either version 2 of the License, or     *
#* (at your option) any later version.                                   *
#*                                                                       *
#* This program is distributed in the hope that it will be useful,       *
#* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
#* GNU General Public License for more details.                          *
#*                                                                       *
#* You should have received a copy of the GNU General Public License     *
#* along with this program; if not, write to the Free Software           *
#* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
#*                                                                       *
#* The author can be reached at admin@atrinik.org                        *
#*************************************************************************
#
# Script to collect archetypes, artifacts, treasures, etc.

import sys, os, getopt, shutil
import xml.etree.ElementTree as ET
from collections import OrderedDict
import re

# Adjust the recursion limit.
sys.setrecursionlimit(50000)
# Various paths.
paths = {
    "root": "../",
}

# Common escape sequences.
class colors:
    bold = "\033[1m"
    underscore = "\033[4m"
    end = "\033[0m"

# Collect archetypes (.arc files).
def collect_archetypes():
    archetypes = open(os.path.join(paths["arch"], "archetypes"), "wb")

    for file in find_files(paths["arch"], ".arc"):
        file_copy(file, archetypes)

    archetypes.close()

# Collect images (.png files).
def collect_images():
    bmaps = open(os.path.join(paths["arch"], "bmaps"), "wb")
    images = open(os.path.join(paths["arch"], "atrinik.0"), "wb")
    num_images = 0
    dev_dir = os.path.join(paths["arch"], "dev")

    # 'bug.101' must be the first entry.
    for file in find_files(dev_dir, "bug.101.png") + sorted(find_files(paths["arch"], ".png", ignore_paths = [dev_dir]), key = lambda path: os.path.basename(path)[:-4]):
        name = os.path.basename(file)[:-4]
        # Write it out to the bmaps file.
        bmaps.write("{0}\n".format(name).encode())

        # Get the image's file size.
        size = os.path.getsize(file)
        fp = open(file, "rb")
        # Write out information about the image to the atrinik.0 file.
        images.write("IMAGE {0} {1} {2}\n".format(num_images, size, name).encode())
        images.write(fp.read())
        fp.close()
        num_images += 1

    bmaps.close()
    images.close()

# Collect animations (.anim files).
def collect_animations():
    l = []

    for file in find_files(paths["arch"], ".anim"):
        fp = open(file, "r")

        for line in fp:
            line = line.strip()

            # Blank line or comment.
            if not line or line[0] == "#":
                continue

            if line.startswith("anim "):
                l.append([line])
            elif line != "mina":
                l[len(l) - 1].append(line)

        fp.close()

    animations = open(os.path.join(paths["arch"], "animations"), "wb")

    for anim in sorted(l, key = lambda anim: anim[0][5:]):
        for line in anim:
            animations.write("{0}\n".format(line).encode())

        animations.write("mina\n".encode())

    animations.close()

# Collect treasures (.trs files in either arch or maps).
def collect_treasures():
    treasures = open(os.path.join(paths["arch"], "treasures"), "wb")

    for file in find_files(paths["arch"], ".trs") + find_files(paths["maps"], ".trs"):
        file_copy(file, treasures)

    treasures.close()

# Collect artifacts (.art files in either art or maps).
def collect_artifacts():
    artifacts = open(os.path.join(paths["arch"], "artifacts"), "wb")

    for file in find_files(paths["arch"], ".art") + find_files(paths["maps"], ".art"):
        file_copy(file, artifacts)

    artifacts.close()

def _collect_parts(file, parent, definition, npcs):
    for part in parent.iterfind("part"):
        if not "parts" in definition:
            definition["parts"] = OrderedDict()

        uid = re.sub(r"\W+", "", part.get("uid"))
        part_def = {
            "name": part.get("name", uid),
            "message": "\n".join(message.text for message in part.iterfind("message")),
        }

        for attr in ["item", "kill"]:
            elem = next(part.iterfind(attr), None)

            if elem is not None:
                part_def[attr] = elem.attrib

                if "nrof" in part_def[attr]:
                    part_def[attr]["nrof"] = int(part_def[attr]["nrof"])

        definition["parts"][uid] = part_def
        _collect_parts(file, part, definition["parts"][uid], npcs)
        _make_interface(file, part, npcs, uid)

def _make_interface(file, parent, npcs, part_uid = None):
    for interface in parent.findall("interface"):
        npc = interface.get("npc")

        if npc:
            npc = re.sub(r"\W+", "", npc.lower().replace(" ", "_"))
        else:
            npc = os.path.basename(file)[:-4]

        if not npc in npcs:
            npcs[npc] = {
                "code": "",
                "quest_uid": None,
                "import": [],
            }

        if parent.tag == "quest":
            npcs[npc]["quest_uid"] = parent.get("uid")

        state = interface.get("state")
        inherit = interface.get("inherit")

        if inherit and inherit.find(".") != -1 and not inherit in npcs[npc]["import"]:
            npcs[npc]["import"].append(inherit)

        if state == None:
            dialog_uid = ""
        else:
            dialog_uid = "_" + state

            if parent.tag == "part":
                dialog_uid += "_" + parent.get("uid")

        if inherit == None:
            interface_inherit = "InterfaceBuilder"
        elif inherit == "":
            interface_inherit = "InterfaceDialog"
        elif inherit.find(".") != -1:
            interface_inherit = inherit[inherit.find(".") + 1:]
        else:
            interface_inherit = "InterfaceDialog_" + inherit

        code = ""
        code += "class InterfaceDialog{dialog_uid}({interface_inherit}):\n".format(**locals())

        class_code = ""

        for action in interface.findall("action"):
            for line in action.text.split("\n"):
                class_code += " " * 4 + line.rstrip() + "\n"

        for dialog in interface.findall("dialog"):
            dialog_name = dialog.get("name", "")

            if dialog_name:
                dialog_name = "_" + dialog_name.replace(" ", "_")
                dialog_args = ""
            else:
                dialog_args = ", msg"

            class_code += " " * 4 + "def dialog{dialog_name}(self{dialog_args}):\n".format(**locals())

            dialog_inherit = "self" if inherit == None else interface_inherit
            dialog_inherit_name = dialog.get("inherit")
            dialog_inherit_args = "" if inherit == None else "self"

            if not dialog_inherit_name:
                dialog_inherit_name = dialog_name
            else:
                dialog_inherit_name = "_" + dialog_inherit_name.replace(" ", "_")

            dialog_inherit_code = " " * 4 * 2 + "{dialog_inherit}.dialog{dialog_inherit_name}({dialog_inherit_args})\n".format(**locals())

            if dialog.get("inherit_process") == "start" or (not dialog.get("inherit_process") and dialog.get("inherit")):
                class_code += dialog_inherit_code

            for elem in dialog:
                if elem.tag == "message":
                    color = elem.get("color", "")

                    if color:
                        color = ", color = {}".format("\"{}\"".format(color[1:]) if color.startswith("#") else "COLOR_{}".format(color.upper()))

                    class_code += " " * 4 * 2 + "self.add_msg(\"{elem.text}\"{color})\n".format(**locals())
                elif elem.tag == "choice":
                    if not "random.choice" in npcs[npc]["import"]:
                        npcs[npc]["import"].append("random.choice")

                    class_code += " " * 4 * 2 + "self.add_msg(choice([{msgs}]))\n".format(msgs = ", ".join("\"{elem.text}\"".format(elem = msg) for msg in elem.findall("message")))
                elif elem.tag == "item":
                    item_args = []

                    for attr, attr2 in [("arch", "archname"), ("name", "name")]:
                        val = elem.get(attr)

                        if val:
                            item_args.append("{attr2} = \"{val}\"".format(**locals()))

                    item_args = ", ".join(item_args)

                    class_code += " " * 4 * 2 + "self.add_objects(me.FindObject({item_args}))\n".format(**locals())

            for response in dialog.findall("response"):
                message = response.get("message")
                link_args = ""

                for attr, attr2 in [("destination", "dest"), ("action", "action")]:
                    val = response.get(attr)

                    if val:
                        link_args += ", {attr2} = \"{val}\"".format(**locals())

                class_code += " " * 4 * 2 + "self.add_link(\"{message}\"{link_args})\n".format(**locals())

            for action in dialog.findall("action"):
                if action.text:
                    for line in action.text.split("\n"):
                        class_code += " " * 4 * 2 + line.rstrip() + "\n"

                for attr in ["start", "complete"]:
                    val = action.get(attr)

                    if val:
                        split = val.split("::")

                        if len(split) > 1:
                            val = "[{}]".format(", ".join("\"{}\"".format(val) for val in split))
                        else:
                            val = "\"{}\"".format(val)

                        class_code += " " * 4 * 2 + "self.qm.{attr}({val})\n".format(**locals())

            if dialog.get("inherit_process") == "end":
                class_code += dialog_inherit_code

        if not class_code:
            class_code += " " * 4 + "pass\n"

        code += class_code

        npcs[npc]["code"] += code

def _dump_quest(definition, indent = 1, pretty = False):
    s = "OrderedDict((" if type(definition) is OrderedDict else "{"

    if pretty:
        s += "\n"

    for key in definition:
        if pretty:
            s += " " * 4 * indent

        if type(definition) is OrderedDict:
            s += "(\"{key}\", ".format(**locals())
        else:
            s += "\"{key}\": ".format(**locals())

        if type(definition[key]) is str:
            s += "\"{}\"".format(definition[key])
        elif type(definition[key]) is int:
            s += "{}".format(definition[key])
        else:
            s += _dump_quest(definition[key], indent + 1)

        if type(definition) is OrderedDict:
            s += ")"

        s += ","

        if pretty:
            s += "\n"

    if pretty:
        s += " " * 4 * (indent - 1)

    s += "))" if type(definition) is OrderedDict else "}"

    return s

def collect_quests():
    quests = open(os.path.join(paths["maps"], "python", "InterfaceQuests.py"), "wb")

    quests.write("from collections import OrderedDict\n")

    for file in find_files(paths["maps"], ".xml"):
        tree = ET.parse(file)
        root = tree.getroot()

        npcs = {}

        _make_interface(file, root, npcs)

        for quest in root.findall("quest"):
            uid = re.sub(r"\W+", "", quest.get("uid"))
            quest_def = {
                "name": quest.get("name"),
                "uid": uid,
            }

            for attr in ["repeat", "repeat_delay"]:
                val = quest.get(attr)

                if val:
                    quest_def[attr] = val

            _make_interface(file, quest, npcs)
            _collect_parts(file, quest, quest_def, npcs)

            quests.write("{} = {}\n".format(uid, _dump_quest(quest_def)))

        for npc in npcs:
            fh = open(os.path.join(os.path.dirname(file), npc + ".py"), "wb+")

            code = [npcs[npc]["code"]]

            if npcs[npc]["quest_uid"]:
                fh.write("from QuestManager import QuestManager\n")
                fh.write("from InterfaceQuests import {}\n".format(npcs[npc]["quest_uid"]))

            for key in npcs[npc]["import"]:
                fh.write("from {} import {}\n".format(*key.split(".")))

            fh.write("from Interface import InterfaceBuilder\n")
            fh.write(npcs[npc]["code"])
            fh.write("ib = InterfaceBuilder(activator, me)\n")

            if npcs[npc]["quest_uid"]:
                fh.write("ib.set_quest(QuestManager(activator, {}))\n".format(npcs[npc]["quest_uid"]))

            fh.write("ib.finish(locals(), msg)\n")

            fh.close()

    quests.close()

# Print usage.
def usage():
    l = []

    # Get the available collectors.
    for entry in dict(globals()):
        if entry.startswith("collect_"):
            l.append(colors.bold + entry[8:] + colors.end)

    print("\n{bold}{underscore}Use:{end}{end}\n\nScript for collecting Atrinik resources like archetypes, treasures, artifacts, etc.\n\n{bold}{underscore}Options:{end}{end}\n\n\t-h, --help:\n\t\tDisplay this help.\n\n\t-c {underscore}type{end}, --collect={underscore}type{end}:\n\t\tWhat to collect. This should be one of {collect_list} (multiple types can be separated by commas). The default is to collect everything. If 'none' is in one of the types, no collection will be done.\n\n\t-d {underscore}directory{end}, --collect={underscore}directory{end}:\n\t\tWhere the root directory of your Atrinik copy is. The default is '../'.\n\n\t-o {underscore}directory{end}, --out={underscore}directory{end}:\n\t\tWhere to copy the (collected) files from the arch directory to (not recursively).".format(**{"bold": colors.bold, "underscore": colors.underscore, "end": colors.end, "collect_list": ", ".join(l[:-2] + [""]) + " and ".join(l[-2:])}))

# Try to parse our command line options.
try:
    opts, args = getopt.getopt(sys.argv[1:], "hc:d:o:", ["help", "collect=", "dir=", "out="])
except getopt.GetoptError as err:
    # Invalid option, show the error, print usage, and exit.
    print(err)
    usage()
    sys.exit(2)

what_collect = []
copy_dest = None

# Parse options.
for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    elif o in ("-c", "--collect"):
        what_collect = []

        for t in a.split(","):
            orig = t.strip()
            t = "collect_" + orig

            if not t in locals() and orig != "none":
                print("No such collect option '{0}'.".format(orig))
                usage()
                sys.exit()

            what_collect.append(t)
    elif o in ("-d", "--dir"):
        paths["root"] = a
    elif o in ("-o", "--out"):
        copy_dest = a

# Find files in the specified path.
# @param where Where to look for the files.
# @param ext What the file must end with.
# @param rec Whether to go on recursively.
# @param ignore_dirs Whether to ignore directories.
# @param ignore_files Whether to ignore files.
# @param ignore_paths What paths to ignore.
# @return A list containing files/directories found based on the set criteria.
def find_files(where, ext = None, rec = True, ignore_dirs = True, ignore_files = False, ignore_paths = None):
    nodes = os.listdir(where)
    files = []

    for node in nodes:
        path = os.path.join(where, node)

        # Do we want to ignore this path?
        if ignore_paths and path in ignore_paths:
            continue

        # A directory.
        if os.path.isdir(path):
            # Do we want to go on recursively?
            if rec:
                files += find_files(path, ext)

            # Are we ignoring directories? If not, add it to the list.
            if not ignore_dirs:
                files.append(path)
        else:
            # Only add the file if we're not ignoring files and ext was not set or it matches.
            if not ignore_files and (not ext or path.endswith(ext)):
                files.append(path)

    return files

# Copy a single file to 'output', stripping whitespace, ignoring empty lines
# and commented out lines.
# @param file File name where to copy from.
# @param output File descriptor where to write to.
def file_copy(file, output):
    fp = open(file, "r")

    for line in fp:
        # Blank line or comment.
        if not line or line[0] == "#":
            continue

        output.write("{0}\n".format(line.rstrip()).encode())

    fp.close()

# Get paths to directories inside the root directory.
for path in find_files(paths["root"], rec = False, ignore_dirs = False, ignore_files = True):
    paths[os.path.basename(path)] = path

if not "collect_none" in what_collect:
    # Nothing was set to collect, so by default we'll collect everything.
    if not what_collect:
        for entry in dict(locals()):
            if entry.startswith("collect_"):
                what_collect.append(entry)

    # Call the collecting functions.
    for collect in what_collect:
        locals()[collect]()

# Copy all files in the arch directory to specified directory.
if copy_dest:
    files = find_files(paths["arch"], rec = False)

    for path in files:
        shutil.copyfile(path, os.path.join(copy_dest, os.path.basename(path)))
