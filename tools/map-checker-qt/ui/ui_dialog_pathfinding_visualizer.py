# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '..\ui\ui_dialog_pathfinding_visualizer.ui'
#
# Created: Wed Feb 11 15:36:09 2015
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
        self.buttonPrev = QtWidgets.QPushButton(DialogPathfindingVisualizer)
        self.buttonPrev.setEnabled(False)
        self.buttonPrev.setAutoRepeat(True)
        self.buttonPrev.setObjectName("buttonPrev")
        self.horizontalLayout.addWidget(self.buttonPrev)
        self.buttonPause = QtWidgets.QPushButton(DialogPathfindingVisualizer)
        self.buttonPause.setEnabled(False)
        self.buttonPause.setObjectName("buttonPause")
        self.horizontalLayout.addWidget(self.buttonPause)
        self.buttonNext = QtWidgets.QPushButton(DialogPathfindingVisualizer)
        self.buttonNext.setEnabled(False)
        self.buttonNext.setAutoRepeat(True)
        self.buttonNext.setObjectName("buttonNext")
        self.horizontalLayout.addWidget(self.buttonNext)
        self.buttonRewind = QtWidgets.QPushButton(DialogPathfindingVisualizer)
        self.buttonRewind.setEnabled(False)
        self.buttonRewind.setObjectName("buttonRewind")
        self.horizontalLayout.addWidget(self.buttonRewind)
        self.label = QtWidgets.QLabel(DialogPathfindingVisualizer)
        self.label.setObjectName("label")
        self.horizontalLayout.addWidget(self.label)
        self.sliderDelay = QtWidgets.QSlider(DialogPathfindingVisualizer)
        self.sliderDelay.setMinimum(1)
        self.sliderDelay.setMaximum(1000)
        self.sliderDelay.setProperty("value", 100)
        self.sliderDelay.setOrientation(QtCore.Qt.Horizontal)
        self.sliderDelay.setObjectName("sliderDelay")
        self.horizontalLayout.addWidget(self.sliderDelay)
        spacerItem = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.horizontalLayout.addItem(spacerItem)
        self.buttonOpen = QtWidgets.QPushButton(DialogPathfindingVisualizer)
        self.buttonOpen.setObjectName("buttonOpen")
        self.horizontalLayout.addWidget(self.buttonOpen)
        self.gridLayout.addLayout(self.horizontalLayout, 3, 0, 1, 1)
        self.graphicsView = QtWidgets.QGraphicsView(DialogPathfindingVisualizer)
        self.graphicsView.setObjectName("graphicsView")
        self.gridLayout.addWidget(self.graphicsView, 0, 0, 1, 1)
        self.horizontalLayout_2 = QtWidgets.QHBoxLayout()
        self.horizontalLayout_2.setObjectName("horizontalLayout_2")
        self.label_2 = QtWidgets.QLabel(DialogPathfindingVisualizer)
        self.label_2.setObjectName("label_2")
        self.horizontalLayout_2.addWidget(self.label_2)
        self.timeTaken = QtWidgets.QLabel(DialogPathfindingVisualizer)
        self.timeTaken.setText("")
        self.timeTaken.setObjectName("timeTaken")
        self.horizontalLayout_2.addWidget(self.timeTaken)
        spacerItem1 = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.horizontalLayout_2.addItem(spacerItem1)
        self.gridLayout.addLayout(self.horizontalLayout_2, 1, 0, 1, 1)
        self.gridLayout_2.addLayout(self.gridLayout, 0, 0, 1, 1)

        self.retranslateUi(DialogPathfindingVisualizer)
        QtCore.QMetaObject.connectSlotsByName(DialogPathfindingVisualizer)

    def retranslateUi(self, DialogPathfindingVisualizer):
        _translate = QtCore.QCoreApplication.translate
        DialogPathfindingVisualizer.setWindowTitle(_translate("DialogPathfindingVisualizer", "Pathfinding Visualizer"))
        self.buttonPrev.setText(_translate("DialogPathfindingVisualizer", "Previous"))
        self.buttonPause.setText(_translate("DialogPathfindingVisualizer", "Pause"))
        self.buttonNext.setText(_translate("DialogPathfindingVisualizer", "Next"))
        self.buttonRewind.setText(_translate("DialogPathfindingVisualizer", "Rewind"))
        self.label.setText(_translate("DialogPathfindingVisualizer", "Delay"))
        self.buttonOpen.setText(_translate("DialogPathfindingVisualizer", "Open"))
        self.label_2.setText(_translate("DialogPathfindingVisualizer", "Pathfinding took:"))

