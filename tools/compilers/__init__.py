class BaseCompiler(object):
    def __init__(self, paths):
        self.paths = paths

    def compile(self):
        raise NotImplementedError("not implemented")

__author__ = 'Alex Tokar'
