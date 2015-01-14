# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '..\ui\ui_dialog_pathfinding_visualizer.ui'
#
# Created: Wed Jan 14 16:57:33 2015
#      by: PyQt5 UI code generator 5.3.2
#
# WARNING! All changes made in this file will be lost!

from PyQt5 import QtCore, QtGui, QtWidgets

class Ui_DialogPathfindingVisualizer(object):
    def setupUi(self, DialogPathfindingVisualizer):
        DialogPathfindingVisualizer.setObjectName("DialogPathfindingVisualizer")
        DialogPathfindingVisualizer.setWindowModality(QtCore.Qt.ApplicationModal)
        DialogPathfindingVisualizer.resize(1024, 768)
        self.gridLayout_2 = QtWidgets.QGridLayout(DialogPathfindingVisualizer)
        self.gridLayout_2.setObjectName("gridLayout_2")
        self.gridLayout = QtWidgets.QGridLayout()
        self.gridLayout.setObjectName("gridLayout")
        self.horizontalLayout = QtWidgets.QHBoxLayout()
        self.horizontalLayout.setObjectName("horizontalLayout")
        spacerItem = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.horizontalLayout.addItem(spacerItem)
        self.buttonOpen = QtWidgets.QPushButton(DialogPathfindingVisualizer)
        self.buttonOpen.setObjectName("buttonOpen")
        self.horizontalLayout.addWidget(self.buttonOpen)
        self.gridLayout.addLayout(self.horizontalLayout, 1, 0, 1, 1)
        self.graphicsView = QtWidgets.QGraphicsView(DialogPathfindingVisualizer)
        self.graphicsView.setObjectName("graphicsView")
        self.gridLayout.addWidget(self.graphicsView, 0, 0, 1, 1)
        self.gridLayout_2.addLayout(self.gridLayout, 0, 0, 1, 1)

        self.retranslateUi(DialogPathfindingVisualizer)
        QtCore.QMetaObject.connectSlotsByName(DialogPathfindingVisualizer)

    def retranslateUi(self, DialogPathfindingVisualizer):
        _translate = QtCore.QCoreApplication.translate
        DialogPathfindingVisualizer.setWindowTitle(_translate("DialogPathfindingVisualizer", "Pathfinding Visualizer"))
        self.buttonOpen.setText(_translate("DialogPathfindingVisualizer", "Open"))

