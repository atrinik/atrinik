'''
Implementation for the 'About' dialog.
'''

from PyQt5.QtWidgets import QDialog

from ui.ui_dialog_about import Ui_DialogAbout
from ui.model import Model


class DialogAbout(Model, QDialog, Ui_DialogAbout):
    '''Implements the About dialog.'''
    def __init__(self, parent = None):
        super(DialogAbout, self).__init__(parent)
        self.setupUi(self)
