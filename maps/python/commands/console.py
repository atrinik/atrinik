## @file
## Implements the /console command.
##
## Only usable by DMs or those with the "console" command permission.

import sys
import code
import collections
import inspect
import re

from Atrinik import *
import Common
from Interface import Interface
from Markup import markup_escape


## Version of the console.
__VERSION__ = "1.3"
## Maximum number of history lines.
__HISTORY__ = 40

class AutoComplete (object):
    def __init__ (self, console):
        self.console = console

    def parse (self, text):
        self.matches = []

        text = re.sub(r"(\w+)(\.[\w\(]*)?", self._objs, text, 1)
        text = re.sub(r"([^=]+)=(\s*\w*)?", self._assignment, text, 1)

        return text, self.matches

    def _best_match (self, l, match = None):
        match_new = ""
        matches = []
        l.sort()

        for entry in l:
            if match is None:
                match = entry
            else:
                for i, c in enumerate(entry):
                    if i >= len(match) or match[i] != c:
                        break

                    match_new += c

                if match_new:
                    match = match_new
                    match_new = ""

            if entry.startswith(match):
                matches.append(match + "[i]" + entry[len(match):] + "[/i]")

        if len(matches) > 1:
            self.matches.append("[b]Possible matches[/b]: {}".format(
                    ", ".join(matches)))

        return match

    def _objs (self, matches):
        obj, prop = matches.groups()
        prop = prop if prop else ""
        objs = []
        props = []

        for name in self.console.locals:
            if not prop and name.startswith(obj):
                objs.append(name)
            elif prop and name == obj:
                prop_name = prop[1:]

                for attribute, val in inspect.getmembers(
                        self.console.locals[name]):
                    if attribute.startswith("_"):
                        continue

                    if prop_name and not attribute.startswith(prop_name):
                        continue

                    props.append("{}{}".format(attribute,
                            "(" if hasattr(val, "__call__") else ""))

                break

        if objs:
            obj = self._best_match(objs)

        if props:
            prop = "." + self._best_match(props, "" if prop == "." else None)

        return obj + prop

    def _assignment (self, matches):
        obj, assignment = matches.groups()
        assignment = assignment.strip() if assignment else ""

        try:
            obj_type = eval("type({})".format(obj), self.console.locals)
        except:
            obj_type = None

        if assignment:
            assignments = []

            for name in self.console.locals:
                if not name.startswith(assignment):
                    continue

                # Do not suggest incompatible variables...
                if hasattr(self.console.locals[name], "__call__") or (
                        obj_type is not None and
                        isinstance(self.console.locals[name], obj_type)):
                    assignments.append("{}{}".format(name,
                            "(" if hasattr(self.console.locals[name],
                            "__call__") else ""))

            if assignments:
                assignment = self._best_match(assignments)
            elif "None".startswith(assignment):
                assignment = "None"
            elif obj_type == bool:
                if "True".startswith(assignment):
                    assignment = "True"
                elif "False".startswith(assignment):
                    assignment = "False"

        return obj + "= " + assignment

# Used to redirect stdout so that it writes the print() and the like
# data to the interface instead of stdout.
class stdout_inf:
    def __init__(self, console):
        self.console = console

    def write(self, s):
        if s != "\n":
            self.console.inf_data += s.split("\n")

    def flush(self):
        self.console.show()

## Handles the console data.
class PyConsole (code.InteractiveConsole):
    def __init__ (self, activator):
        self.activator = activator

        _locals = {
            "__name__": console_name(activator),
            "__doc__": None,
            "activator": activator,
            "me": activator,
            "find_obj": Common.find_obj,
        }
        # The console name will have the activator's name for uniqueness.
        code.InteractiveConsole.__init__(self, locals = _locals)

        # Create a ring buffer.
        self.inf_data = collections.deque(maxlen = __HISTORY__)

        self.inf_data.append("Atrinik Python Console v{}".format(__VERSION__))
        self.inf_data.append("Use exit() to exit the session.")

        self.stdout = stdout_inf(self)
        self.ac = AutoComplete(self)

    def is_valid (self):
        if not self.activator:
            return False

        return True

    def write (self, data):
        self.inf_data.append(data)
        self.show()

    def push (self, data):
        do_flush = True

        if data.startswith("noinf::"):
            data = data[7:]
            do_flush = False

        if data.startswith("ac::"):
            data = data[4:]
            ac, matches = self.ac.parse(data)
            self.show(ac, "\n".join(matches))
            return

        self.inf_data.append(">>> {}".format(data))
        old_stdout = sys.stdout
        sys.stdout = self.stdout
        code.InteractiveConsole.push(self, data)

        if do_flush:
            sys.stdout.flush()

        sys.stdout = old_stdout

    def show (self, ac = None, append = None):
        inf = Interface(self.activator, self.activator)

        if ac is None:
            inf.set_title(self.activator.name + "'s Python Console")
            msg = markup_escape("\n".join(self.inf_data))
            inf.add_msg("[font=mono 12]{}[/font]".format(msg))
        else:
            inf.restore()

        inf.set_text_input(prepend = "/console \"", allow_tab = True,
                allow_empty = True, cleanup_text = False, scroll_bottom = True,
                autocomplete = "noinf::ac::",
                text = ac if ac else "")

        if append:
            inf.set_append_text("[font=mono 12]\n{}[/font]".format(append))

        inf.send()

def console_name(activator):
    return "__console-" + activator.name.lower() + "__"

def console_find(activator):
    key = console_name(activator)

    try:
        console = CacheGet(key)
        assert(isinstance(console, PyConsole))

        if console.is_valid():
            return console
        else:
            CacheRemove(key)
    except ValueError:
        pass

    return None

def console_create(activator):
    console = PyConsole(activator)
    CacheAdd(console_name(activator), console)

    return console

def console_remove(activator):
    CacheRemove(console_name(activator))

def main():
    msg = WhatIsMessage()

    # The hidden prepend keyword of the text input also includes
    # double-quotes so that left whitespace is not stripped by the
    # server.
    if msg and msg.startswith("\""):
        msg = msg[1:]

    Logger("CHAT", "Console: {}: {}".format(activator.name, msg))

    console = console_find(activator)

    if msg == "exit()":
        if console:
            console_remove(activator)
            inf = Interface(activator, None)
            inf.dialog_close()

        return

    if not console:
        console = console_create(activator)

    console.push(msg)

    if not msg:
        console.show()

main()
