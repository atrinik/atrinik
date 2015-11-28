"""
Implements the preferences dialog.
"""

from PyQt5.QtWidgets import QDialog, QWidget, QLineEdit, QToolButton, \
    QFileDialog, QCheckBox

from ui.model import Model
from ui.ui_dialog_preferences import Ui_DialogPreferences


class DialogPreferences(Model, QDialog, Ui_DialogPreferences):
    """Preferences dialog implementation."""

    def __init__(self, parent=None):
        super(DialogPreferences, self).__init__(parent)
        self.setupUi(self)

        self.buttonOk.clicked.connect(self.buttonOkTrigger)
        self.buttonApply.clicked.connect(self.buttonApplyTrigger)

        for widget in self.findChildren(QToolButton):
            if widget.objectName().startswith("fileChooser_"):
                widget.clicked.connect(self.makeFileChooser(
                    widget.objectName()[len("fileChooser_"):]))

    def show(self):
        super(DialogPreferences, self).show()
        self.settings_parse()

    def settings_parse(self, save=False):
        for section in self.config.sections():
            for option in self.config.options(section):
                name = "pref_{}_{}".format(section.replace(" ", "_").lower(),
                                           option)
                widget = self.findChild(QWidget, name)

                if not widget:
                    continue

                if type(widget) == QLineEdit:
                    if save:
                        self.config.set(section, option, widget.text())
                    else:
                        widget.setText(self.config.get(section, option))
                elif type(widget) == QCheckBox:
                    if save:
                        self.config.set(section, option,
                                        "yes" if widget.isChecked() else "no")
                    else:
                        widget.setChecked(
                            self.config.getboolean(section, option))

    def buttonOkTrigger(self):
        self.buttonApplyTrigger()
        self.hide()

    def buttonApplyTrigger(self):
        self.settings_parse(True)

    def makeFileChooser(self, name):
        def actionFileChooser():
            setting = name.split("_")
            option = "_".join(setting[2:])
            widget = self.findChild(QLineEdit, "_".join(setting))

            if option.startswith("path_dir_"):
                path = QFileDialog.getExistingDirectory(self,
                                                        "Select Directory",
                                                        widget.text(),
                                                        QFileDialog.ShowDirsOnly | QFileDialog.DontResolveSymlinks)
            elif option.startswith("path_file_"):
                path = \
                QFileDialog.getOpenFileName(self, "Select File", widget.text())[
                    0]

            if not path:
                return

            widget.setText(path)

        return actionFileChooser
