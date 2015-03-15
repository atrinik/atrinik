"""
Implementation for the 'Interface Editor' window.
"""

import os
import xml.etree.ElementTree as ElementTree
import system.utils
import xml.dom.minidom

from PyQt5 import QtWidgets, QtGui
from PyQt5 import QtCore
from PyQt5.QtCore import Qt, QItemSelectionModel
from PyQt5.QtWidgets import QMainWindow
from ui.ui_window_interface_editor import Ui_WindowInterfaceEditor
from ui.model import Model


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
            item = self

            for i in rows:
                item = item.item(i)

            items.append(item)

        tree_view = self.parent().treeView

        for item in items:
            parent = item.parent()

            if not parent:
                parent = self

            expanded = []

            if tree_view.isExpanded(self.indexFromItem(item)):
                expanded.append(item)

            for item2 in self.parent().model_items(item=item):
                if tree_view.isExpanded(self.indexFromItem(item2)):
                    expanded.append(item2)

            i = item.row()
            old_row = i
            item = parent.takeRow(i)[0]

            if row == -1:
                target.appendRow(item)
            else:
                if target == parent and row >= i:
                    i = row - 1
                else:
                    i = row

                target.insertRow(i, item)

            if item.row() != old_row:
                item.modified = True

            for item2 in expanded:
                tree_view.expand(self.indexFromItem(item2))

        return False

class WindowInterfaceEditor(Model, QMainWindow, Ui_WindowInterfaceEditor):
    """Implements the Interface Editor window."""

    def __init__(self, parent=None):
        super(WindowInterfaceEditor, self).__init__(parent)
        self.setupUi(self)

        self._last_path = None
        self.last_item = None
        self.file_path = None

        self.interface_elements = InterfaceElementCollection()

        self.model = ItemModel(self)
        self.treeView.setModel(self.model)
        self.treeView.setContextMenuPolicy(Qt.CustomContextMenu)
        self.treeView.customContextMenuRequested.connect(
            self.tree_view_open_menu)
        self.treeView.clicked.connect(self.treeViewTrigger)

        self.actionNew.triggered.connect(self.action_new_trigger)
        self.actionOpen.triggered.connect(self.action_open_trigger)
        self.actionSave.triggered.connect(self.action_save_trigger)
        self.actionSave_As.triggered.connect(self.action_save_as_trigger)
        
        self.reset_stacked_widget()

    @property
    def last_path(self):
        if self._last_path is None:
            return os.path.join(self.map_checker.getMapsPath(), "interfaces")

        return self._last_path

    @last_path.setter
    def last_path(self, value):
        self._last_path = value

    def prompt_unsaved(self):
        reply = QtWidgets.QMessageBox.question(
            self, self.tr("Are you sure?"),
            self.tr("You have unsaved changes in the current interface file. "
                    "Are you sure you want to proceed?"),
            QtWidgets.QMessageBox.Yes, QtWidgets.QMessageBox.No)

        if reply == QtWidgets.QMessageBox.No:
            return False

        return True

    def check_unsaved(self):
        self.save_last_item()

        for item in self.model_items():
            if item.modified:
                return self.prompt_unsaved()

        return True

    def action_new_trigger(self):
        if not self.check_unsaved():
            return

        self.reset_stacked_widget()
        self.model.clear()
        self.file_path = None

    def action_open_trigger(self):
        if not self.check_unsaved():
            return

        path = QtWidgets.QFileDialog.getOpenFileName(
            self, self.tr("Select Interface File"), self.last_path,
            self.tr("Interface files (*.xml)"))

        if not path or not path[0]:
            return

        path = path[0]
        self.last_path = os.path.dirname(path)
        self.load_interface_file(path)

    def action_save_trigger(self):
        if self.file_path is None:
            self.action_save_as_trigger()
            return

        self.save_interface_file(self.file_path)

    def action_save_as_trigger(self):
        path = QtWidgets.QFileDialog.getSaveFileName(
            self, self.tr("Save Interface File"),
            self.file_path or self.last_path,
            self.tr("Interface files (*.xml)"))

        if not path or not path[0]:
            return

        path = path[0]
        self.last_path = os.path.dirname(path)
        self.file_path = path
        self.action_save_trigger()

    def save_last_item(self):
        if self.last_item is not None:
            self.last_item.saveData()

    def save_interface_file(self, path):
        self.save_last_item()

        elem = ElementTree.Element("interfaces")
        self.model_items_apply(self.model_items_apply_to_xml, (elem,))

        xmlstr = ElementTree.tostring(elem, "utf-8")
        xmlstr = xml.dom.minidom.parseString(xmlstr)

        with open(path, "wb") as f:
            f.write(xmlstr.toprettyxml(indent=" " * 4, encoding="UTF-8"))

    def reset_stacked_widget(self):
        self.stackedWidget.setCurrentIndex(0)
        self.last_item = None

    def fill_model_from_xml(self, elem, parent=None):
        if parent is None:
            parent = self.model

            for child in elem:
                self.fill_model_from_xml(child, self.model)

            return

        item = self.interface_elements[elem.tag]()
        item.fillData(elem)
        parent.appendRow(item)

        for child in elem:
            self.fill_model_from_xml(child, item)

    def load_interface_file(self, path):
        self.reset_stacked_widget()
        self.model.clear()
        self.file_path = path

        try:
            tree = ElementTree.parse(path)
        except ElementTree.ParseError as e:
            print("Error parsing {}: {}".format(path, e))
            return

        self.fill_model_from_xml(tree.getroot())
        self.treeView.expandAll()

    def model_items(self, item=None):
        if item is None:
            item = self.model

        i = 0

        while item.item(i):
            yield item.item(i)

            for child in self.model_items(item=item.item(i)):
                yield child

            i += 1

    def model_items_apply(self, fnc, args=(), item=None):
        if item is None:
            item = self.model

        i = 0

        while item.item(i):
            child = item.item(i)
            ret = fnc(child, *args)
            self.model_items_apply(fnc, ret, child)
            i += 1

    @staticmethod
    def model_items_apply_to_xml(item, parent):
        elem = item.build_xml_element()
        item.modified = False
        parent.append(elem)
        return elem,

    def treeViewTrigger(self):
        index = self.treeView.selectedIndexes()[-1]
        item = self.treeView.model().itemFromIndex(index)

        if self.last_item is not None:
            self.last_item.saveData()

        item.switchTo()
        self.last_item = item

    def tree_view_open_menu(self, position):
        menu = QtWidgets.QMenu()
        submenu = menu.addMenu("New")

        for element in self.interface_elements.sorted():
            action = QtWidgets.QAction(self.tr(element.tag.capitalize()), self)
            action.triggered.connect(lambda ignored, cls=element:
                                     self.tree_view_handle_new(cls))
            submenu.addAction(action)

        deleteAction = QtWidgets.QAction(self.tr("Delete"), self)
        deleteAction.triggered.connect(self.tree_view_handle_delete)
        menu.addAction(deleteAction)
        menu.exec_(self.treeView.viewport().mapToGlobal(position))

    def tree_view_handle_delete(self):
        for index in self.treeView.selectedIndexes():
            item = self.model.itemFromIndex(index)

            if self.last_item == item:
                self.reset_stacked_widget()

            parent = item.parent() or self.model
            parent.takeRow(item.row())

    def tree_view_handle_new(self, cls):
        if not self.treeView.selectedIndexes():
            self.model.appendRow(cls())
            return

        for index in self.treeView.selectedIndexes():
            item = self.model.itemFromIndex(index)
            item.appendRow(cls())


class InterfaceElement(QtGui.QStandardItem):
    tag = "none"
    attributes = ()
    dialog_attributes = ()

    def __init__ (self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._data = {}
        self._modified = True
        self.updateText()

    @property
    def modified(self):
        return self._modified

    @modified.setter
    def modified(self, value):
        assert isinstance(value, bool)
        self._modified = value
        self.updateText()

    @property
    def window(self):
        return self.model().parent()

    def item(self, i):
        return self.child(i)

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
            elif isinstance(widget, QtWidgets.QCheckBox):
                if val:
                    widget.setChecked(bool(int(val)))
                else:
                    widget.setChecked(False)
            else:
                raise NotImplementedError("Unknown widget: {}".format(widget))

        for child in page.findChildren(QtWidgets.QPushButton):
            if child.objectName().startswith("interface_button_dialog_"):
                name = "_".join(child.objectName().split("_")[3:])
                child.clicked.connect(lambda: self.dialog_button_trigger(name)) # TODO: fix me

    def updateText(self, s=""):
        text = "{}{}{}".format("* " if self.modified else "", self.tag, s)
        self.setText(text)

    def fillData(self, elem):
        self.modified = False

        for attr in self.attributes:
            if attr == "text":
                val = elem.text
                elem.text = None
            else:
                val = elem.get(attr)

                if val is not None:
                    del elem.attrib[attr]

            if val is None:
                continue

            self._data[attr] = val.strip()

        if elem.text and elem.text.strip():
            print("Element {}: XML tag text was not handled: '{}'".format(
                elem, elem.text))

        for attr in elem.attrib:
            print("Element {}: Attribute {} was not handled: '{}'".format(
                elem, attr, elem.attrib[attr]))

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
            elif isinstance(widget, QtWidgets.QCheckBox):
                val = "1" if widget.isChecked() else ""
            else:
                raise NotImplementedError("Unknown widget: {}".format(widget))

            val = val.strip()

            if not val and attr in self._data:
                del self._data[attr]
                self.modified = True
            elif self._data.get(attr, "") != val:
                self._data[attr] = val
                self.modified = True

    def build_xml_element(self):
        elem = ElementTree.Element(self.tag)

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
    attributes = ("name", "repeat", "repeat_delay")
    priority = 500

    def updateText(self):
        name = self._data.get("name")
        text = ""

        if name:
            text += " ({})".format(name)

        super().updateText(text)


class InterfaceElementInterface(InterfaceElement):
    tag = "interface"
    attributes = ("state", "npc", "inherit")
    priority = 100

    def updateText(self):
        state = self._data.get("state")
        npc = self._data.get("npc")
        name = self._data.get("name")
        text = ""

        if npc:
            text += " for {}".format(npc)

        if state:
            text += " ({})".format(state)

        super().updateText(text)


class InterfaceElementDialog(InterfaceElement):
    tag = "dialog"
    attributes = ("name", "regex", "inherit", "icon", "title")
    priority = 200

    def updateText(self):
        name = self._data.get("name")
        text = ""

        if name:
            text += " ({})".format(name)

        super().updateText(text)


class InterfaceElementMessage(InterfaceElement):
    tag = "message"
    attributes = ("color", "text")
    priority = 300


class InterfaceElementAnd(InterfaceElement):
    tag = "and"
    priority = 1000


class InterfaceElementCheck(InterfaceElement):
    tag = "check"
    attributes = ("region_map", "enemy", "started", "finished", "completed",
                  "num2finish", "options")
    priority = 1000


class InterfaceElementResponse(InterfaceElement):
    tag = "response"
    attributes = ("message", "destination", "action")
    dialog_attributes = ("destination",)
    priority = 400


class InterfaceElementAction(InterfaceElement):
    tag = "action"
    attributes = ("region_map", "start", "complete", "enemy", "text",
                  "teleport", "trigger")
    priority = 1000

    def build_xml_element(self):
        elem = super().build_xml_element()

        if elem.text:
            elem.text = elem.text.replace("\t", " " * 4)

        return elem


class InterfaceElementNotification(InterfaceElement):
    tag = "notification"
    attributes = ("message", "action", "shortcut", "delay")
    priority = 1000


class InterfaceElementInherit(InterfaceElement):
    tag = "inherit"
    attributes = ("name",)
    priority = 1000


class InterfaceElementPart(InterfaceElement):
    tag = "part"
    attributes = ("uid", "name")
    priority = 600

    def updateText(self):
        uid = self._data.get("name")
        name = self._data.get("name")
        text = ""

        if uid or name:
            text += " ("

            if uid:
                text += uid

            if uid and name:
                text += " - "

            if name:
                text += name

            text += ")"

        super().updateText(text)


class InterfaceElementInfo(InterfaceElement):
    tag = "info"
    attributes = ("text",)
    priority = 1000


class InterfaceElementItem(InterfaceElement):
    tag = "item"
    attributes = ("arch", "name", "nrof", "keep")
    priority = 1000


class InterfaceElementObject(InterfaceElement):
    tag = "object"
    attributes = ("arch", "name", "remove", "message")
    priority = 1000


class InterfaceElementChoice(InterfaceElement):
    tag = "choice"
    priority = 1000


class InterfaceElementKill(InterfaceElement):
    tag = "kill"
    attributes = ("nrof",)
    priority = 1000


class InterfaceElementSay(InterfaceElement):
    tag = "say"
    attributes = ("text",)
    priority = 1000


class InterfaceElementClose(InterfaceElement):
    tag = "close"
    priority = 1000


class InterfaceElementCollection(object):
    def __init__(self):
        self.elements = {}

        for cls in system.utils.itersubclasses(InterfaceElement):
            assert cls.tag not in self.elements, "{} already exists".format(
                cls.tag)
            self.elements[cls.tag] = cls

    def __getitem__(self, key):
        return self.elements[key]

    def __len__(self):
        return len(self.elements)

    def sorted(self):
        for tag in sorted(self.elements,
                          key=lambda tag: (self.elements[tag].priority, tag)):
            yield self.elements[tag]
