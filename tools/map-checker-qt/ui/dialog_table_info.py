"""
Implements the table info dialog.

This dialog is brought up by double-clicking an entry in the list of
errors, and contains extra information about the error.
"""

from PyQt5.QtWidgets import QDialog

from ui.model import Model
from ui.ui_dialog_table_info import Ui_DialogTableInfo


class DialogTableInfo(Model, QDialog, Ui_DialogTableInfo):
    """
    Implements the table row info dialog.
    """

    def __init__(self, parent=None):
        super(DialogTableInfo, self).__init__(parent)
        self.setupUi(self)

    def update_data(self, items):
        """
        Updates the dialog with info about the clicked table row.
        @param items List of row items.
        """

        self.infoFile_name.setText(items[0].data["file"]["name"])
        self.infoFile_path.setText(items[0].data["file"]["path"])
        self.infoSeverity.setText(items[1].text())
        self.infoDescription.setText(items[0].data["description"])
        self.infoExplanation.setText(items[0].data["explanation"])
