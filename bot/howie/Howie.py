import random, os

try:
    import howie.configFile, howie.core
    enabled = True
except ImportError:
    print("WARNING: Could not import Howie, chatbot will be disabled.")
    enabled = False

class Howie:
    def _change_dir(self):
        self._cwd = os.getcwd()
        os.chdir(os.path.dirname(__file__))

    def _restore_dir(self):
        os.chdir(self._cwd)

    def __init__(self):
        if not enabled:
            return

        random.seed()
        self._change_dir()
        config = howie.configFile.load("howie.ini")
        howie.core.init()
        self._restore_dir()

    def submit(self, msg, name, session):
        if not enabled:
            return "This function is not available at this time."

        self._change_dir()
        ret = howie.core.submit(msg, session)
        self._restore_dir()
        return ret
