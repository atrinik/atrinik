## @file
## Implements the /console command.
##
## Only usable by DMs or those with the "console" command permission.

from Interface import Interface
import Common
import threading, code

## Version of the console.
__VERSION__ = "1.2"
## The beginning of the thread name.
__THREADNAME__ = "PyConsoleThread-"
## Maximum number of history lines.
__HISTORY__ = 40
## Time to sleep in the thread loop.
__SLEEPTIME__ = 0.05
## Lock used for stdout.
stdout_lock = threading.Lock()

## Handles the console data.
class PyConsole(code.InteractiveConsole):
    def __init__(self, activator):
        _locals = {
            "__name__": "__console-" + activator.name.lower() + "__",
            "__doc__": None,
            "activator": activator,
            "me": activator,
            "find_obj": Common.find_obj,
        }
        # The thread name will have the activator's name for uniqueness.
        code.InteractiveConsole.__init__(self, locals = _locals)
        matches = []

    def write(self, data):
        self.inf_data.append(data)

## The actual function that is called in the per-player console thread.
def py_console_thread():
    import time, collections, sys, re, inspect
    from Markup import markup_escape

    # Sends an interface to the activator.
    def send_inf(activator, msg, autocomplete = None, append_text = None):
        inf = Interface(activator, None)

        if msg:
            inf.set_icon(activator.face[0])
            inf.set_title(activator.name + "'s Python Console")
            inf.add_msg("[font=mono 12]" + msg + "[/font]")
        else:
            inf.restore()

        inf.set_text_input(prepend = "/console \"", allow_tab = True, allow_empty = True, cleanup_text = False, scroll_bottom = True, autocomplete = "noinf::ac::", text = autocomplete if autocomplete else "")

        if append_text:
            inf.set_append_text("[font=mono 12]\n" + append_text + "[/font]")

        inf.finish()

    def autocomplete_best_match(l, match = None):
        match_new = ""
        matches = []
        l.sort()

        for entry in l:
            if match == None:
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
            console.matches.append("[b]Possible matches[/b]: {}".format(", ".join(matches)))

        return match

    def autocomplete_objs(matches):
        obj, prop = matches.groups()
        prop = prop if prop else ""
        objs = []
        props = []

        for name in console.locals:
            if not prop and name.startswith(obj):
                objs.append(name)
            elif prop and name == obj:
                prop_name = prop[1:]

                for attribute, val in inspect.getmembers(console.locals[name]):
                    if attribute.startswith("_"):
                        continue

                    if prop_name and not attribute.startswith(prop_name):
                        continue

                    props.append("{}{}".format(attribute, "(" if hasattr(val, "__call__") else ""))

                break

        if objs:
            obj = autocomplete_best_match(objs)

        if props:
            prop = "." + autocomplete_best_match(props, "" if prop == "." else None)

        return obj + prop

    def autocomplete_assignment(matches):
        obj, assignment = matches.groups()
        assignment = assignment.strip() if assignment else ""

        try:
            obj_type = eval("type({})".format(obj), console.locals)
        except:
            obj_type = None

        if assignment:
            assignments = []

            for name in console.locals:
                if not name.startswith(assignment):
                    continue

                # Do not suggest incompatible variables...
                if hasattr(console.locals[name], "__call__") or (obj_type != None and isinstance(console.locals[name], obj_type)):
                    assignments.append("{}{}".format(name, "(" if hasattr(console.locals[name], "__call__") else ""))

            if assignments:
                assignment = autocomplete_best_match(assignments)
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
        def __init__(self, inf_data):
            self._inf_data = inf_data

        def write(self, s):
            if s != "\n":
                self._inf_data += s.split("\n")

        def flush(self):
            pass

    # Create a ring buffer.
    inf_data = collections.deque(maxlen = __HISTORY__)
    # Get the current thread.
    thread = threading.current_thread()

    # Create the console.
    console = PyConsole(thread.activator)
    console.inf_data = inf_data

    stdout = stdout_inf(inf_data)

    # Send the greeting message.
    inf_data.append("Atrinik Python Console v{}".format(__VERSION__))
    inf_data.append("Use exit() to exit the session.")

    # Now loop until we get killed.
    while not thread.killed():
        # Activator object is no longer valid, break out.
        if not thread.activator:
            break

        # Acquire the commands lock.
        thread.commands_lock.acquire()

        if thread.commands:
            # Acquire the stdout lock.
            stdout_lock.acquire()
            # Redirect stdout.
            old_stdout = sys.stdout
            sys.stdout = stdout

            for command in thread.commands:
                if command == None:
                    continue

                if command.startswith("noinf::"):
                    command = command[7:]

                if command.startswith("ac::"):
                    console.matches = []
                    text_input = re.sub(r"(\w+)(\.\w*)?", autocomplete_objs, command[4:], 1)
                    text_input = re.sub(r"([^=]+)=(\s*\w*)?", autocomplete_assignment, text_input, 1)
                    send_inf(thread.activator, None, text_input, "\n".join(console.matches))
                    continue

                inf_data.append(">>> {}".format(command))
                console.push(command)

            sys.stdout = old_stdout
            stdout_lock.release()

            # Send the interface, but only if the first command didn't
            # start with "noinf::".
            if not thread.commands[0] or not thread.commands[0].startswith("noinf::"):
                send_inf(thread.activator, markup_escape("\n".join(inf_data)))

            thread.commands = []

        # Release the lock.
        thread.commands_lock.release()
        # Save CPU.
        time.sleep(__SLEEPTIME__)

## Setups the console thread.
class PyConsoleThread(threading.Thread):
    def __init__(self, activator):
        # The thread name will have the activator's name for uniqueness.
        super(PyConsoleThread, self).__init__(name = __THREADNAME__ + activator.name, target = py_console_thread)
        self.activator = activator
        self.commands = []
        self.commands_lock = threading.Lock()
        self._kill = threading.Event()

    def kill(self):
        self._kill.set()

    def killed(self):
        return self._kill.is_set()

## Tries to find an existing console thread.
def find_thread():
    for thread in threading.enumerate():
        if thread.name == __THREADNAME__ + activator.name:
            return thread

def main():
    msg = WhatIsMessage()

    # The hidden prepend keyword of the text input also includes
    # double-quotes so that left whitespace is not stripped by the
    # server.
    if msg and msg.startswith("\""):
        msg = msg[1:]

    Logger("CHAT", "Console: {}: {}".format(activator.name, msg))

    # Try to find the thread.
    thread = find_thread()

    if msg == "exit()":
        # Kill the existing thread.
        if thread:
            thread.kill()
            inf = Interface(activator, None)
            inf.dialog_close()

        return

    if not thread:
        # Thread doesn't exist yet, create it.
        thread = PyConsoleThread(activator)
        thread.start()

    thread.commands_lock.acquire()
    thread.commands.append(msg if WhatIsMessage() else None)
    thread.commands_lock.release()

main()
