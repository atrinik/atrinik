import os
import queue
import subprocess
import sys
import webbrowser
import socket
import struct

from PyQt5 import QtGui, QtCore, QtWidgets
from PyQt5.QtCore import QTimer
from PyQt5.QtWidgets import QMainWindow, QTableWidgetItem, QTableWidget, \
    QStyleOptionViewItem, QLabel, QStyledItemDelegate

import system.constants
from system.constants import game
from system.utils import html2text
from ui.dialog_about import DialogAbout
from ui.dialog_preferences import DialogPreferences
from ui.dialog_table_info import DialogTableInfo
from ui.dialog_pathfinding_visualizer import DialogPathfindingVisualizer
from ui.window_interface_editor import WindowInterfaceEditor
from ui.model import Model
from ui.ui_window_main import Ui_WindowMain


class WindowMain(Model, QMainWindow, Ui_WindowMain):
    def __init__(self):
        # Initialize the super-class and setup the UI
        super(WindowMain, self).__init__()
        self.setupUi(self)

        self.exit_fnc = None

        self.progressBar.setVisible(False)

        # Some variables.
        self.dialogs = {}
        self.last_scan_directory = None
        self.error_levels = system.constants.ErrorLevelCollection()
        self.widgetTables = {
            "maps": self.widgetTableMaps,
            "resources": self.widgetTableResources
        }

        # Create some dialogs.
        self.dialogs["about"] = DialogAbout(self)
        self.dialogs["preferences"] = DialogPreferences(self)
        self.dialogs["table_info"] = DialogTableInfo(self)
        self.dialogs["pathfinding_visualizer"] = DialogPathfindingVisualizer(
            self)
        self.dialogs["interface_editor"] = WindowInterfaceEditor(self)

        self.actionScan.triggered.connect(self.actionScanTrigger)
        self.actionScan_directory.triggered.connect(
            self.actionScan_directoryTrigger)
        self.actionExit.triggered.connect(self.actionExitTrigger)

        self.actionSelect_all.triggered.connect(self.actionSelect_allTrigger)
        self.actionPreferences.triggered.connect(self.actionPreferencesTrigger)

        self.actionOpen_selected_in_editor.triggered.connect(
            self.actionOpen_selected_in_editorTrigger)
        self.actionOpen_selected_in_client.triggered.connect(
            self.actionOpen_selected_in_clientTrigger)

        self.actionPathfinding_Visualizer.triggered.connect(
            self.actionPathfinding_VisualizerTrigger)
        self.actionInterface_Editor.triggered.connect(
            self.actionInterface_EditorTrigger)

        self.actionReport_a_problem.triggered.connect(
            self.actionReport_a_problemTrigger)
        self.actionAbout.triggered.connect(self.actionAboutTrigger)

        self.buttonOpen_all.clicked.connect(self.buttonOpen_allTrigger)

        for table in self.widgetTables:
            table = self.widgetTables[table]
            table.setEditTriggers(QTableWidget.NoEditTriggers)
            table.itemDoubleClicked.connect(self.widgetTableTrigger)

            delegate = RichTextColumnDelegate(table)
            table.setItemDelegateForColumn(1, delegate)

            table.setColumnWidth(0, 200)
            table.setColumnWidth(1, 100)

        self.timer = QTimer()
        self.timer.timeout.connect(self.mapCheckerQueueProcess)
        self.timer.start(100)

    def setExitFunction(self, fnc):
        self.exit_fnc = fnc

    def closeEvent(self, ev):
        if self.exit_fnc:
            self.exit_fnc()

    def set_config(self, config):
        super(WindowMain, self).set_config(config)

        for dialog in self.dialogs:
            self.dialogs[dialog].set_config(config)
            self.dialogs[dialog].main_window = self
            self.dialogs[dialog].setMapChecker(self.map_checker)

    def getVisibleWidgetTable(self):
        if self.widgetTabs.currentIndex() == 0:
            return self.widgetTables["maps"]
        else:
            return self.widgetTables["resources"]

    def getTabName(self, idx):
        name = self.widgetTabs.tabText(idx)
        space = name.find(" ")

        if space != -1:
            return name[:space]

        return name

    def open_maps(self, maps):
        # Get default environment variables.
        envs = dict(os.environ)
        delimiter = ";" if sys.platform.startswith("win") else ":"
        # Extend the PATH environment variable with the script's dir.
        envs["PATH"] += delimiter + self.map_checker.path
        # Execute Gridarta.
        subprocess.Popen(["java", "-jar", self.config.get("General",
                                                          "path_file_editor")] + list(
            maps), cwd=os.path.dirname(
            self.config.get("General", "path_file_editor")), env=envs,
                         shell=sys.platform.startswith("win"))

    def open_client(self, path, loc):
        if loc is None:
            x = y = 0
        else:
            x, y = loc

        app_name = "Atrinik Map Checker"

        packets = []

        packet = struct.pack("!B", game.server_commands.control)
        packet += app_name.encode("ascii") + b"\0"
        packet += struct.pack("!2B", game.server_commands.control_map,
                              game.server_commands.control_map_reset)
        packet += path.encode("ascii") + b"\0"
        packets.append(packet)

        packet = struct.pack("!B", game.server_commands.control)
        packet += app_name.encode("ascii") + b"\0"
        packet += struct.pack("!2B", game.server_commands.control_player,
                              game.server_commands.control_player_teleport)
        packet += b"\0"
        packet += path.encode("ascii") + b"\0"
        packet += struct.pack("!2H", x, y)
        packets.append(packet)

        s = socket.create_connection(("localhost", 13327), 5.0)

        for packet in packets:
            s.sendall(struct.pack("!2B", (len(packet) >> 8) & 0xff,
                                  len(packet) & 0xff) + packet)

        s.close()

    def widgetTableTrigger(self, item):
        table = self.getVisibleWidgetTable()
        items = [table.item(item.row(), x) for x in range(table.columnCount())]

        self.dialogs["table_info"].show()
        self.dialogs["table_info"].update_data(items)

    def mapCheckerQueueProcess(self):
        if self.map_checker.scan_is_running():
            self.progressBar.setVisible(True)
            self.progressBar.setValue(
                self.map_checker.scan_get_progress() * self.progressBar.maximum())
            self.statusLabel.setText(self.map_checker.scan_get_status())
        else:
            self.progressBar.setVisible(False)
            self.statusLabel.setText("")

        text = "Stop" if self.map_checker.scan_is_running() else "Scan"

        if self.actionScan.text() != text:
            for widget in [self.actionScan, self.actionScan_directory,
                           self.buttonScan]:
                if widget != self.actionScan_directory:
                    widget.setText(text)
                elif text != "Scan":
                    continue

                widget.setEnabled(True)

        while self.map_checker.queue.qsize():
            try:
                error = self.map_checker.queue.get(0)

                if error["file"]["is_map"]:
                    table = self.widgetTables["maps"]
                    tab_idx = 0
                else:
                    table = self.widgetTables["resources"]
                    tab_idx = 1

                row = table.rowCount()

                table.insertRow(row)
                table.setItem(row, 0,
                              WidgetTableItem(error["file"]["name"], error))
                table.setItem(row, 1, WidgetTableItem(
                    "<font color='{}'>{}</font>".format(
                        self.error_levels[error["severity"]].getColor("qt"),
                        error["severity"])))
                table.setItem(row, 2,
                              WidgetTableItem(html2text(error["description"])))

                self.widgetTabs.setTabText(tab_idx, "{} ({})".format(
                    self.getTabName(tab_idx), row + 1))
            except queue.Empty:
                pass

    def actionScanTrigger(self, path=None):
        if self.map_checker.scan_is_running():
            self.map_checker.scan_stop()
        else:
            for table in self.widgetTables:
                self.widgetTables[table].setRowCount(0)

            for i in range(self.widgetTabs.count()):
                self.widgetTabs.setTabText(i, self.getTabName(i))

            self.map_checker.scan(path,
                                  fix=self.config.getboolean("General", "fix"))

        self.actionScan.setEnabled(False)
        self.actionScan_directory.setEnabled(False)
        self.buttonScan.setEnabled(False)

    def actionExitTrigger(self):
        self.close()

    def actionScan_directoryTrigger(self):
        if not self.last_scan_directory:
            self.last_scan_directory = self.map_checker.get_maps_path()

        path = QtWidgets.QFileDialog.getExistingDirectory(self,
                                                          "Select Directory",
                                                          self.last_scan_directory,
                                                          QtWidgets.QFileDialog.ShowDirsOnly | QtWidgets.QFileDialog.DontResolveSymlinks)

        if not path:
            return

        self.last_scan_directory = path
        self.actionScanTrigger(path)

    def actionReport_a_problemTrigger(self):
        webbrowser.open(system.constants.urls.report_bug)

    def actionAboutTrigger(self):
        self.dialogs["about"].show()

    def actionPreferencesTrigger(self):
        self.dialogs["preferences"].show()

    def actionOpen_selected_in_editorTrigger(self):
        maps = set(
            [self.widgetTableMaps.item(y, 0).data["file"]["path"] for y in
             set([x.row() for x in self.widgetTableMaps.selectedItems()])])
        self.open_maps(maps)

    def actionOpen_selected_in_clientTrigger(self):
        selected = set([x.row() for x in self.widgetTableMaps.selectedItems()])

        if len(selected) != 1:
            return

        row, = selected
        data = self.widgetTableMaps.item(row, 0).data
        path = "/" + os.path.relpath(data["file"]["path"],
                                     self.map_checker.get_maps_path()).replace(
            os.path.sep, "/")
        self.open_client(path, data["loc"])

    def actionPathfinding_VisualizerTrigger(self):
        self.dialogs["pathfinding_visualizer"].show()

    def actionInterface_EditorTrigger(self):
        self.dialogs["interface_editor"].show()

    def actionSelect_allTrigger(self):
        self.getVisibleWidgetTable().selectAll()

    def buttonOpen_allTrigger(self):
        maps = set(
            [self.widgetTableMaps.item(i, 0).data["file"]["path"] for i in
             range(self.widgetTableMaps.rowCount())])
        self.open_maps(maps)


class RichTextColumnDelegate(QStyledItemDelegate):
    """Implements delegate for showing error level column in tables."""

    def paint(self, painter, option, index):
        text = str(index.model().data(index, QtCore.Qt.DisplayRole))

        if (index.row(), index.column()) in [(x.row(), x.column()) for x in
                                             self.parent().selectedItems()]:
            style = 'background: palette(highlight); color: palette(highlighted-text);'
            text = html2text(text)
        else:
            style = 'background: white;'

        style += 'text-transform: uppercase; font-family: "Segoe UI"; font-size: 9pt'

        label = QLabel()
        label.setStyleSheet(style)

        text = "<center><b>{}</b></center>".format(text)

        label.setText(text)
        label.setFixedSize(option.rect.size())
        label.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        label.setMargin(5)

        painter.save()
        painter.translate(option.rect.x(), option.rect.y())
        label.render(painter)
        painter.restore()

    def sizeHint(self, option, index):
        options = QStyleOptionViewItem(option)
        self.initStyleOption(options, index)

        doc = QtGui.QTextDocument()
        doc.setHtml(options.text)
        doc.setTextWidth(options.rect.width())

        return QtCore.QSize(doc.idealWidth() + 5, option.fontMetrics.height())


class WidgetTableItem(QTableWidgetItem):
    """Implements custom item class for table widget items."""

    def __init__(self, text, data=None):
        super(WidgetTableItem, self).__init__(text)
        self.data = data
