'''
Implementation for the 'Interface Editor' window.
'''

import os
import xml.etree.ElementTree as ET
import system.utils
import xml.dom.minidom

from PyQt5 import QtWidgets, QtGui
from PyQt5 import QtCore
from PyQt5.QtCore import QTimer, Qt, QItemSelectionModel, QAbstractItemModel
from PyQt5.QtWidgets import QMainWindow, QGraphicsScene, QApplication
from ui.ui_window_interface_editor import Ui_WindowInterfaceEditor
from ui.model import Model

interface_elements = {}

class ItemModel(QtGui.QStandardItemModel):
    def supportedDragActions(self):
        return Qt.MoveAction

    def mimeTypes(self):
        return ["application/atrinik.map-checker.interface-element"]

    def mimeData(self, indices):
        mime_data = QtCore.QMimeData()
        encoded_data = QtCore.QByteArray()
        stream = QtCore.QDataStream(encoded_data, QtCore.QIODevice.WriteOnly)

        for index in indices:
            if not index.isValid():
                continue

            item = self.itemFromIndex(index)
            rows = []

            parent = item.parent()

            while parent:
                rows.append(parent.row())
                parent = parent.parent()

            rows.reverse()
            rows.append(item.row())
            stream << QtCore.QVariant(rows)

        mime_data.setData("application/atrinik.map-checker.interface-element",
                          encoded_data)
        return mime_data

    def dropMimeData(self, data, action, row, column, parent):
        print("row: {}".format(row))
        print("parent: {}".format(parent))

        if parent and parent.isValid():
            target = self.itemFromIndex(parent)
        else:
            target = self

        encoded_data = data.data(
            "application/atrinik.map-checker.interface-element")
        stream = QtCore.QDataStream(encoded_data, QtCore.QIODevice.ReadOnly)

        items = []

        while not stream.atEnd():
            variant = QtCore.QVariant()
            stream >> variant
            rows = variant.value()
            i = rows.pop(0)
            item = self.item(i)

            for i in rows:
                item = item.child(i)

            items.append(item)

        for item in items:
            parent = item.parent()

            if not parent:
                parent = self

            i = item.row()
            item = parent.takeRow(i)

            if row == -1:
                target.appendRow(item)
            else:
                if target == parent and row >= i:
                    i = row - 1
                else:
                    i = row

                target.insertRow(i, item)

        return False

class WindowInterfaceEditor(Model, QMainWindow, Ui_WindowInterfaceEditor):
    '''Implements the Interface Editor window.'''

    def __init__(self, parent=None):
        super(WindowInterfaceEditor, self).__init__(parent)
        self.setupUi(self)

        self.last_path = None
        self.last_item = None
        self.file_path = None

        self.model = ItemModel(self)
        self.treeView.setModel(self.model)

        self.buttonOpen.clicked.connect(self.buttonOpenTrigger)
        self.buttonSave.clicked.connect(self.buttonSaveTrigger)
        self.treeView.clicked.connect(self.treeViewTrigger)

        self.button_expand_all.clicked.connect(self.button_expand_all_trigger)

    def parseInterfaceFileAdd(self, parent, elem):
        item = interface_elements[elem.tag]()
        item.fillData(elem)
        parent.appendRow(item)

        for subelem in elem:
            self.parseInterfaceFileAdd(item, subelem)

    def parseInterfaceFile(self, path):
        self.stackedWidget.setCurrentIndex(0)
        self.model.clear()
        self.last_item = None
        self.file_path = path

        try:
            tree = ET.parse(path)
        except ET.ParseError as e:
            print("Error parsing {}: {}".format(path, e))
            return
        
        root = tree.getroot()

        for elem in root:
            self.parseInterfaceFileAdd(self.model, elem)

    def saveTreeViewAdd(self, parent, item):
        elem = item.build_xml_element()
        parent.append(elem)

        i = 0

        while item.child(i):
            self.saveTreeViewAdd(elem, item.child(i))
            i += 1

    def saveTreeView(self):
        elem = ET.Element("interfaces")

        i = 0

        while self.model.item(i):
            self.saveTreeViewAdd(elem, self.model.item(i))
            i += 1

        return elem

    def buttonSaveTrigger(self):
        if self.last_item is not None:
            self.last_item.saveData()

        elem = self.saveTreeView()

        xmlstr = ET.tostring(elem, "utf-8")
        xmlstr = xml.dom.minidom.parseString(xmlstr)

        with open(self.file_path, "wb") as f:
            f.write(xmlstr.toprettyxml(indent=" " * 4, encoding="UTF-8"))

    def buttonOpenTrigger(self):
        if self.last_path is None:
            self.last_path = os.path.join(self.map_checker.getMapsPath(), "interfaces")

        path = QtWidgets.QFileDialog.getOpenFileName(self, "Select Interface File", self.last_path, "Interface files (*.xml)")

        if not path:
            return

        path = path[0]
        self.last_path = os.path.dirname(path)
        self.parseInterfaceFile(path)

    def treeViewTrigger(self):
        index = self.treeView.selectedIndexes()[0]
        item = self.treeView.model().itemFromIndex(index)

        if self.last_item is not None:
            self.last_item.saveData()

        item.switchTo()
        self.last_item = item

    def button_expand_all_trigger(self):
        self.treeView.expandAll()


class InterfaceElement(QtGui.QStandardItem):
    tag = "none"
    attributes = ()
    dialog_attributes = ()

    def clone(self):
        super().clone()
        print("cloned")

    def __init__ (self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._data = {}
        self.updateText()

    @property
    def window(self):
        return self.model().parent()

    def get_widget(self, attr):
        name = "interface_element_{}_{}".format(self.tag, attr)
        return self.window.__getattribute__(name)

    def get_dialogs(self):
        i = 0

        while self.parent().parent().child(i):
            item = self.parent().parent().child(i)

            if isinstance(item, InterfaceElementDialog):
                yield item

            i += 1

    def dialog_button_trigger(self, attr):
        widget = self.get_widget(attr)
        name = widget.currentText()

        for item in self.get_dialogs():
            if item._data.get("name") == name:
                self.window.treeView.selectionModel().setCurrentIndex(
                    self.window.treeView.model().indexFromItem(item),
                    QItemSelectionModel.ClearAndSelect)
                self.window.treeViewTrigger()
                break

    def switchTo(self):
        stacked_widget = self.window.findChild(QtWidgets.QStackedWidget,
                                               "stackedWidget")
        page = stacked_widget.findChild(QtWidgets.QWidget,
                                        "page" + self.tag.capitalize())
        stacked_widget.setCurrentIndex(stacked_widget.indexOf(page))

        for attr in self.dialog_attributes:
            widget = self.get_widget(attr)
            widget.clear()

            for item in self.get_dialogs():
                widget.addItem(item._data.get("name"))

        for attr in self.attributes:
            widget = self.get_widget(attr)
            val = self._data.get(attr)

            if isinstance(widget, QtWidgets.QPlainTextEdit):
                widget.setPlainText(val)
            elif isinstance(widget, QtWidgets.QLineEdit):
                widget.setText(val)
            elif isinstance(widget, QtWidgets.QComboBox):
                widget.setCurrentText(val)
            else:
                raise NotImplementedError("Unknown widget: {}".format(widget))

        for child in page.findChildren(QtWidgets.QPushButton):
            if child.objectName().startswith("interface_button_dialog_"):
                name = "_".join(child.objectName().split("_")[3:])
                child.clicked.connect(lambda: self.dialog_button_trigger(name))


    def updateText(self):
        self.setText(self.tag)

    def fillData(self, elem):
        for attr in self.attributes:
            self._data[attr] = elem.text if attr == "text" else elem.get(attr)

        self.updateText()

    def saveData(self):
        for attr in self.attributes:
            widget = self.get_widget(attr)

            if isinstance(widget, QtWidgets.QPlainTextEdit):
                val = widget.toPlainText()
            elif isinstance(widget, QtWidgets.QLineEdit):
                val = widget.text()
            elif isinstance(widget, QtWidgets.QComboBox):
                val = widget.currentText()
            else:
                raise NotImplementedError("Unknown widget: {}".format(widget))

            self._data[attr] = val

    def build_xml_element(self):
        elem = ET.Element(self.tag)

        for attr in self.attributes:
            val = self._data.get(attr)

            if not val:
                continue

            if attr == "text":
                elem.text = val
            else:
                elem.set(attr, val)

        return elem


class InterfaceElementQuest(InterfaceElement):
    tag = "quest"
    attributes = ("name",)

    def updateText(self):
        self.setText("{} ({})".format(self.tag, self._data.get("name")))


class InterfaceElementInterface(InterfaceElement):
    tag = "interface"
    attributes = ("state", "npc", "inherit")

    def updateText(self):
        state = self._data.get("state")
        npc = self._data.get("npc")
        s = self.tag

        if npc:
            s += " for {}".format(npc)

        if state:
            s += " ({})".format(state)

        self.setText(s)


class InterfaceElementDialog(InterfaceElement):
    tag = "dialog"
    attributes = ("name", "regex", "inherit")

    def updateText(self):
        s = self.tag
        name = self._data.get("name")

        if name:
            s += " ({})".format(name)

        self.setText(s)


class InterfaceElementMessage(InterfaceElement):
    tag = "message"
    attributes = ("color", "text")


class InterfaceElementAnd(InterfaceElement):
    tag = "and"


class InterfaceElementCheck(InterfaceElement):
    tag = "check"
    attributes = ("region_map", "enemy", "started", "finished", "completed",
                  "num2finish", "options")



class InterfaceElementResponse(InterfaceElement):
    tag = "response"
    attributes = ("message", "destination", "action")
    dialog_attributes = ("destination",)


class InterfaceElementAction(InterfaceElement):
    tag = "action"
    attributes = ("region_map", "start", "complete", "enemy", "text")

    def build_xml_element(self):
        elem = super().build_xml_element()

        if elem.text:
            elem.text = elem.text.replace("\t", " " * 4)

        return elem


class InterfaceElementNotification(InterfaceElement):
    tag = "notification"
    attributes = ("message", "action", "shortcut", "delay")


class InterfaceElementInherit(InterfaceElement):
    tag = "inherit"
    attributes = ("name",)


class InterfaceElementPart(InterfaceElement):
    tag = "part"
    attributes = ("uid", "name")

    def updateText(self):
        self.setText("{} ({} - {})".format(self.tag, self._data.get("uid"),
                                           self._data.get("name")))


class InterfaceElementInfo(InterfaceElement):
    tag = "info"
    attributes = ("text",)


class InterfaceElementItem(InterfaceElement):
    tag = "item"
    attributes = ("arch", "name", "nrof")


class InterfaceElementObject(InterfaceElement):
    tag = "object"
    attributes = ("name",)


class InterfaceElementChoice(InterfaceElement):
    tag = "choice"


class InterfaceElementKill(InterfaceElement):
    tag = "kill"
    attributes = ("nrof",)


class InterfaceElementSay(InterfaceElement):
    tag = "say"
    attributes = ("text",)


class InterfaceElementClose(InterfaceElement):
    tag = "close"

for cls in system.utils.itersubclasses(InterfaceElement):
    interface_elements[cls.tag] = cls
