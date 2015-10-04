"""
Implementation for the 'Interface Editor' window.
"""

import os
import subprocess
import xml.etree.ElementTree as ElementTree
import xml.dom.minidom
import logging

from PyQt5 import QtWidgets, QtGui
from PyQt5 import QtCore
from PyQt5.QtCore import Qt, QItemSelectionModel
from PyQt5.QtWidgets import QMainWindow
import sys

import system.utils
from ui.ui_window_interface_editor import Ui_WindowInterfaceEditor
from ui.model import Model

INTERFACES_DOCTYPE = """<!DOCTYPE interfaces PUBLIC
"-//Atrinik//ADS-1 1.1.2//EN" "ads-1.dtd">"""


class ItemModel(QtGui.QStandardItemModel):
    MIME_TYPE = "application/atrinik.map-checker.interface-element"

    def supportedDragActions(self):
        return Qt.MoveAction

    def mimeTypes(self):
        return [self.MIME_TYPE]

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

            while parent != self:
                rows.append(parent.row())
                parent = parent.parent()

            rows.reverse()
            rows.append(item.row())
            stream << QtCore.QVariant(rows)

        mime_data.setData(self.MIME_TYPE, encoded_data)
        return mime_data

    def dropMimeData(self, data, action, row, column, parent):
        if parent and parent.isValid():
            target = self.itemFromIndex(parent)
        else:
            target = self

        encoded_data = data.data(self.MIME_TYPE)
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

        command = CommandMove(items, target, row, self.parent(),
                              "Move {} elements".format(len(items)))
        self.parent().undo_stack.push(command)

        return False


class Command(QtWidgets.QUndoCommand):
    def __init__(self, window, description):
        super().__init__(description)
        self.window = window


class CommandNew(Command):
    def __init__(self, targets, cls, *args):
        super().__init__(*args)
        self.targets = targets
        # noinspection PyUnusedLocal
        self.items = [cls() for target in targets]

    def redo(self):
        self.window.logger.debug("Redo new command: %s, %s", self.targets,
                                 self.items)

        for i, target in enumerate(self.targets):
            target.appendRow(self.items[i])

            if target == self.window.model:
                continue

            self.window.treeView.expand(self.window.model.indexFromItem(target))

    def undo(self):
        self.window.logger.debug("Undo new command: %s, %s", self.targets,
                                 self.items)

        for item in self.items:
            item.parent().takeRow(item.row())


class CommandMove(Command):
    def __init__(self, items, target, row, *args):
        super().__init__(*args)
        self.items = items
        self.target = target
        self.row = row
        self.positions = [(item.parent(), item.row()) for item in items]

    def redo(self):
        self.window.logger.debug("Redo move command: %s, %s, %s, %s",
                                 self.items, self.target, self.row,
                                 self.positions)

        for item in self.items:
            self.window.item_insert(item, self.target, self.row)

    def undo(self):
        self.window.logger.debug("Redo move command: %s, %s, %s, %s",
                                 self.items, self.target, self.row,
                                 self.positions)

        for i, item in enumerate(self.items):
            self.window.item_insert(item, *self.positions[i])


class CommandDelete(Command):
    def __init__(self, items, *args):
        super().__init__(*args)
        self.items = items
        self.positions = [(item.parent(), item.row(),
                           self.window.save_expanded(item)) for item in items]

    def redo(self):
        self.window.logger.debug("Redo delete command: %s, %s", self.items,
                                 self.positions)

        for item in self.items:
            if self.window.last_item == item:
                self.window.reset_stacked_widget()

            item.parent().takeRow(item.row())

    def undo(self):
        self.window.logger.debug("Undo delete command: %s, %s", self.items,
                                 self.positions)

        for i, item in enumerate(self.items):
            self.window.item_insert(item, *self.positions[i])


class CommandSaveData(Command):
    def __init__(self, item, *args):
        super().__init__(*args)
        self.item = item
        self.item_data = item.elem_data.copy()
        self.real_redo = False

    def redo(self):
        self.window.logger.debug("Redo save data command: %s, %s", self.item,
                                 self.item_data)

        old_data = self.item.elem_data.copy()
        self.item.elem_data = self.item_data.copy()
        self.item_data = old_data

        if self.real_redo:
            self.item.switch_to()
            self.window.treeView.selectionModel().setCurrentIndex(
                self.window.model.indexFromItem(self.item),
                QtCore.QItemSelectionModel.NoUpdate)
        else:
            self.item.save_data()

    def undo(self):
        self.window.logger.debug("Undo save data command: %s, %s", self.item,
                                 self.item_data)

        if self.window.last_item is not None:
            self.window.last_item.save_data()

        old_data = self.item.elem_data.copy()
        self.item.elem_data = self.item_data.copy()
        self.item_data = old_data
        self.real_redo = True
        self.item.switch_to()
        self.window.treeView.selectionModel().setCurrentIndex(
            self.window.model.indexFromItem(self.item),
            QtCore.QItemSelectionModel.NoUpdate)
        self.window.last_item = self.item


class CommandPaste(Command):
    def __init__(self, targets, items, *args):
        super().__init__(*args)
        self.targets = targets
        self.items = []

        # noinspection PyUnusedLocal
        for target in targets:
            self.items.append([item.clone() for item in items])

    def redo(self):
        self.window.logger.debug("Redo paste command: %s, %s", self.targets,
                                 self.items)

        for i, target in enumerate(self.targets):
            for item in self.items[i]:
                target.appendRow(item)

            if target == self.window.model:
                continue

            self.window.treeView.expand(self.window.model.indexFromItem(target))

    def undo(self):
        self.window.logger.debug("Undo paste command: %s, %s", self.targets,
                                 self.items)

        for i, target in enumerate(self.targets):
            for item in self.items[i]:
                item.parent().takeRow(item.row())


class WindowInterfaceEditor(Model, QMainWindow, Ui_WindowInterfaceEditor):
    """Implements the Interface Editor window."""

    def __init__(self, parent=None):
        super(WindowInterfaceEditor, self).__init__(parent)
        self.setupUi(self)

        self.logger = logging.getLogger("interface-editor")

        self._last_path = None
        self.last_item = None
        self.file_path = None
        self.to_paste = []

        self.interface_elements = InterfaceElementCollection()

        self.undo_stack = QtWidgets.QUndoStack()

        self.model = ItemModel(self)
        self.treeView.setModel(self.model)
        self.treeView.setContextMenuPolicy(Qt.CustomContextMenu)

        self.reset_stacked_widget()
        self.setup_connections()

    # noinspection PyUnresolvedReferences
    def setup_connections(self):
        """Sets up action (click, trigger, etc) connections."""

        self.undo_stack.undoTextChanged.connect(self.undo_text_changed_trigger)
        self.undo_stack.redoTextChanged.connect(self.redo_text_changed_trigger)

        self.treeView.customContextMenuRequested.connect(
            self.tree_view_open_menu
        )
        self.treeView.clicked.connect(self.tree_view_trigger)

        self.actionNew.triggered.connect(self.action_new_trigger)
        self.actionOpen.triggered.connect(self.action_open_trigger)
        self.actionSave.triggered.connect(self.action_save_trigger)
        self.actionSave_As.triggered.connect(self.action_save_as_trigger)

        self.actionUndo.triggered.connect(self.action_undo_trigger)
        self.actionRedo.triggered.connect(self.action_redo_trigger)
        self.actionCut.triggered.connect(self.action_cut_trigger)
        self.actionCopy.triggered.connect(self.action_copy_trigger)
        self.actionPaste.triggered.connect(self.action_paste_trigger)
        self.actionDelete.triggered.connect(self.action_delete_trigger)
        self.actionSelect_All.triggered.connect(self.action_select_all_trigger)

    @property
    def last_path(self):
        if self._last_path is None:
            return os.path.join(self.map_checker.get_maps_path(), "interfaces")

        return self._last_path

    @last_path.setter
    def last_path(self, value):
        self._last_path = value

    def save_expanded(self, item):
        expanded = []

        if self.treeView.isExpanded(self.model.indexFromItem(item)):
            expanded.append(item)

        for item2 in self.model_items(item=item):
            if self.treeView.isExpanded(self.model.indexFromItem(item2)):
                expanded.append(item2)

        return expanded

    def restore_expanded(self, expanded):
        for item2 in expanded:
            self.treeView.expand(self.model.indexFromItem(item2))

    def item_insert(self, item, target, row, expanded=None):
        parent = item.parent()

        if expanded is None:
            expanded = self.save_expanded(item)

        if parent:
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
        else:
            target.insertRow(row, item)
            item.modified = True

        self.restore_expanded(expanded)

    def prompt_unsaved(self):
        # noinspection PyTypeChecker,PyCallByClass
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

    def action_undo_trigger(self):
        self.logger.debug("Undo trigger")
        self.undo_stack.undo()

    def action_redo_trigger(self):
        self.logger.debug("Redo trigger")
        self.undo_stack.redo()

    def action_cut_trigger(self):
        self.logger.debug("Cut trigger")
        items = []

        for index in self.treeView.selectedIndexes():
            items.append(self.model.itemFromIndex(index))

        command = CommandDelete(items, self, "Cut {} element(s)".format(
            len(items)))
        self.undo_stack.push(command)
        self.to_paste = items

    def action_copy_trigger(self):
        self.logger.debug("Copy trigger")
        items = []

        for index in self.treeView.selectedIndexes():
            items.append(self.model.itemFromIndex(index))

        self.to_paste = items

    def action_paste_trigger(self):
        if not self.to_paste:
            return

        self.logger.debug("Paste trigger")

        if not self.treeView.selectedIndexes():
            targets = [self.model]
        else:
            targets = [self.model.itemFromIndex(index) for index in
                       self.treeView.selectedIndexes()]

        command = CommandPaste(targets, self.to_paste, self,
                               "Paste {} element(s)".format(len(self.to_paste)))
        self.undo_stack.push(command)

    def action_delete_trigger(self):
        self.logger.debug("Delete trigger")
        items = []

        for index in self.treeView.selectedIndexes():
            items.append(self.model.itemFromIndex(index))

        command = CommandDelete(items, self, "Delete {} element(s)".format(
            len(items)))
        self.undo_stack.push(command)

    def action_select_all_trigger(self):
        self.logger.debug("Select All trigger")
        self.treeView.selectAll()

    def undo_text_changed_trigger(self, text):
        self.actionUndo.setText(self.tr("Undo {}".format(text)))

    def redo_text_changed_trigger(self, text):
        self.actionRedo.setText(self.tr("Redo {}".format(text)))

    def model_clear(self):
        self.logger.debug("Clearing the model")
        self.reset_stacked_widget()
        self.model.clear()
        self.undo_stack.clear()

    def action_new_trigger(self):
        if not self.check_unsaved():
            return

        self.model_clear()
        self.file_path = None

    def action_open_trigger(self):
        if not self.check_unsaved():
            return

        # noinspection PyTypeChecker,PyCallByClass
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

        if self.config.getboolean("Interface Editor", "collect"):
            path = self.config.get("General", "path_dir_tools")
            p = subprocess.Popen(["python", "collect.py", "-c", "interfaces"],
                                 cwd=path, shell=sys.platform.startswith("win"))
            p.wait()

    def action_save_as_trigger(self):
        # noinspection PyTypeChecker,PyCallByClass
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
            self.last_item.save_data()

    def save_interface_file(self, path):
        self.save_last_item()

        self.logger.debug("Saving %s", path)

        elem = ElementTree.Element("interfaces")
        self.model_items_apply(self.model_items_apply_to_xml, (elem,))

        xmlstr = INTERFACES_DOCTYPE.encode("utf-8")
        xmlstr += ElementTree.tostring(elem, "utf-8")
        dom = xml.dom.minidom.parseString(xmlstr)

        with open(path, "wb") as f:
            f.write(dom.toprettyxml(indent=" " * 4, encoding="UTF-8"))

    def reset_stacked_widget(self):
        self.stackedWidget.setCurrentIndex(0)
        self.last_item = None

    def fill_model_from_xml(self, elem, parent=None):
        if parent is None:
            for child in elem:
                self.fill_model_from_xml(child, self.model)

            return

        item = self.interface_elements[elem.tag]()
        item.fill_data(elem)
        parent.appendRow(item)

        for child in elem:
            self.fill_model_from_xml(child, item)

    def load_interface_file(self, path):
        self.model_clear()
        self.file_path = path

        try:
            tree = ElementTree.parse(path)
        except ElementTree.ParseError as e:
            print("Error parsing {}: {}".format(path, e))
            return

        self.logger.debug("Loading %s", path)

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

    def tree_view_trigger(self):
        index = self.treeView.selectedIndexes()[-1]
        item = self.treeView.model().itemFromIndex(index)

        if self.last_item is not None and self.last_item.save_data(True):
            command = CommandSaveData(self.last_item, self,
                                      "Modify '{}'".format(self.last_item.tag))
            self.undo_stack.push(command)

        item.switch_to()
        self.last_item = item

    def tree_view_open_menu(self, position):
        menu = QtWidgets.QMenu()
        submenu = menu.addMenu("New")

        for element in self.interface_elements.sorted():
            action = QtWidgets.QAction(self.tr(element.tag.capitalize()), self)
            # noinspection PyUnresolvedReferences
            action.triggered.connect(lambda ignored, cls=element:
                                     self.tree_view_handle_new(cls))
            submenu.addAction(action)

        delete_action = QtWidgets.QAction(self.tr("Delete"), self)
        # noinspection PyUnresolvedReferences
        delete_action.triggered.connect(self.action_delete_trigger)
        menu.addAction(delete_action)
        menu.exec_(self.treeView.viewport().mapToGlobal(position))

    def tree_view_handle_new(self, cls):
        if not self.treeView.selectedIndexes():
            targets = [self.model]
        else:
            targets = [self.model.itemFromIndex(index) for index in
                       self.treeView.selectedIndexes()]

        command = CommandNew(targets, cls, self, "New '{}'".format(cls.tag))
        self.undo_stack.push(command)


class InterfaceElement(QtGui.QStandardItem):
    tag = "none"
    attributes = ()
    dialog_attributes = ()

    def __init__(self, *args):
        super().__init__(*args)
        self.elem_data = {}
        self._modified = True
        self.update_text()

    def clone(self):
        """
        Creates a new interface element, copying the element data, attributes,
        flags, etc.

        This is necessary because Qt Widgets cannot be deep-copied.
        :return: Cloned InterfaceElement.
        :rtype InterfaceElement
        """
        obj = type(self)()
        obj._modified = self._modified
        obj.elem_data = self.elem_data.copy()
        return obj

    @property
    def modified(self):
        return self._modified

    @modified.setter
    def modified(self, value):
        assert isinstance(value, bool)
        self._modified = value
        self.update_text()

    @property
    def window(self):
        return self.model().parent()

    def parent(self):
        parent = super().parent()

        if parent is None:
            return self.model()

        return parent

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
            if item.elem_data.get("name") == name:
                self.window.treeView.selectionModel().setCurrentIndex(
                    self.window.treeView.model().indexFromItem(item),
                    QItemSelectionModel.ClearAndSelect)
                self.window.tree_view_trigger()
                break

    def switch_to(self):
        stacked_widget = self.window.findChild(QtWidgets.QStackedWidget,
                                               "stackedWidget")
        page = stacked_widget.findChild(QtWidgets.QWidget,
                                        "page" + self.tag.capitalize())
        stacked_widget.setCurrentIndex(stacked_widget.indexOf(page))

        for attr in self.dialog_attributes:
            widget = self.get_widget(attr)
            widget.clear()

            for item in self.get_dialogs():
                widget.addItem(item.elem_data.get("name"))

        for attr in self.attributes:
            widget = self.get_widget(attr)
            val = self.elem_data.get(attr)

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
                child.clicked.connect(lambda ignored, dialog=name:
                                      self.dialog_button_trigger(dialog))

    def update_text(self, s=""):
        text = "{}{}{}".format("* " if self.modified else "", self.tag, s)
        self.setText(text)

    def fill_data(self, elem):
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

            self.elem_data[attr] = val.strip()

        if elem.text and elem.text.strip():
            print("Element {}: XML tag text was not handled: '{}'".format(
                elem, elem.text))

        for attr in elem.attrib:
            print("Element {}: Attribute {} was not handled: '{}'".format(
                elem, attr, elem.attrib[attr]))

        self.update_text()

    def save_data(self, dry=False):
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

            if not val and attr in self.elem_data:
                if dry:
                    return True

                del self.elem_data[attr]
                self.modified = True
            elif self.elem_data.get(attr, "") != val:
                if dry:
                    return True

                self.elem_data[attr] = val
                self.modified = True

        return False

    def build_xml_element(self):
        elem = ElementTree.Element(self.tag)

        for attr in self.attributes:
            val = self.elem_data.get(attr)

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

    def update_text(self, s=""):
        name = self.elem_data.get("name")
        text = ""

        if name:
            text += " ({})".format(name)

        super().update_text(text)


class InterfaceElementInterface(InterfaceElement):
    tag = "interface"
    attributes = ("state", "npc", "inherit")
    priority = 100

    def update_text(self, s=""):
        state = self.elem_data.get("state")
        npc = self.elem_data.get("npc")
        text = ""

        if npc:
            text += " for {}".format(npc)

        if state:
            text += " ({})".format(state)

        super().update_text(text)


class InterfaceElementDialog(InterfaceElement):
    tag = "dialog"
    attributes = ("name", "regex", "inherit", "icon", "title", "animation")
    priority = 200

    def update_text(self, s=""):
        name = self.elem_data.get("name")
        text = ""

        if name:
            text += " ({})".format(name)

        super().update_text(text)


class InterfaceElementMessage(InterfaceElement):
    tag = "message"
    attributes = ("color", "text")
    priority = 300


class InterfaceElementAnd(InterfaceElement):
    tag = "and"
    priority = 1000


class InterfaceElementOr(InterfaceElement):
    tag = "or"
    priority = 1000


class InterfaceElementCheck(InterfaceElement):
    tag = "check"
    attributes = ("region_map", "enemy", "started", "finished", "completed",
                  "num2finish", "options", "gender")
    priority = 1000


class InterfaceElementNcheck(InterfaceElement):
    tag = "ncheck"
    attributes = ("region_map", "enemy", "started", "finished", "completed",
                  "num2finish", "options", "gender")
    priority = 1000


class InterfaceElementResponse(InterfaceElement):
    tag = "response"
    attributes = ("message", "destination", "action")
    dialog_attributes = ("destination",)
    priority = 400

    def update_text(self, s=""):
        destination = self.elem_data.get("destination")
        action = self.elem_data.get("action")
        text = ""

        if destination:
            text += " ({})".format(destination)

        if action:
            text += " (action: {})".format(action)

        super().update_text(text)


class InterfaceElementAction(InterfaceElement):
    tag = "action"
    attributes = ("region_map", "start", "complete", "enemy", "text",
                  "teleport", "trigger", "cast")
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

    def update_text(self, s=""):
        uid = self.elem_data.get("uid")
        name = self.elem_data.get("name")
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

        super().update_text(text)


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


class InterfaceElementCode(InterfaceElement):
    tag = "code"
    attributes = ("text",)
    priority = 1000


class InterfaceElementPrecond(InterfaceElement):
    tag = "precond"
    attributes = ("text",)
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
        for elem in sorted(self.elements,
                           key=lambda tag: (self.elements[tag].priority, tag)):
            yield self.elements[elem]
