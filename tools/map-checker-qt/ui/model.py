'''
UI model implementation, inherited by all dialogs/windows/etc.
'''

class Model:
    '''The UI model.'''

    def set_config(self, config):
        '''Sets config.'''
        self.config = config

    def show(self):
        super().show()
        self.move(self.frameGeometry().topLeft())

    def setMapChecker(self, map_checker):
        self.map_checker = map_checker
