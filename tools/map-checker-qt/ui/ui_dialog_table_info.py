# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '..\ui\ui_dialog_table_info.ui'
#
# Created: Tue Nov 12 16:04:55 2013
#      by: PyQt5 UI code generator 5.1.1
#
# WARNING! All changes made in this file will be lost!

from PyQt5 import QtCore, QtGui, QtWidgets

class Ui_DialogTableInfo(object):
    def setupUi(self, DialogTableInfo):
        DialogTableInfo.setObjectName("DialogTableInfo")
        DialogTableInfo.resize(525, 323)
        DialogTableInfo.setModal(True)
        self.gridLayout_2 = QtWidgets.QGridLayout(DialogTableInfo)
        self.gridLayout_2.setObjectName("gridLayout_2")
        self.gridLayout = QtWidgets.QGridLayout()
        self.gridLayout.setObjectName("gridLayout")
        self.gridLayout_3 = QtWidgets.QGridLayout()
        self.gridLayout_3.setObjectName("gridLayout_3")
        self.infoDescription = QtWidgets.QTextEdit(DialogTableInfo)
        self.infoDescription.setReadOnly(True)
        self.infoDescription.setObjectName("infoDescription")
        self.gridLayout_3.addWidget(self.infoDescription, 1, 0, 1, 1)
        self.infoExplanation = QtWidgets.QTextEdit(DialogTableInfo)
        self.infoExplanation.setReadOnly(True)
        self.infoExplanation.setObjectName("infoExplanation")
        self.gridLayout_3.addWidget(self.infoExplanation, 1, 2, 1, 1)
        spacerItem = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.gridLayout_3.addItem(spacerItem, 1, 1, 1, 1)
        self.label = QtWidgets.QLabel(DialogTableInfo)
        self.label.setObjectName("label")
        self.gridLayout_3.addWidget(self.label, 0, 0, 1, 1)
        self.label_2 = QtWidgets.QLabel(DialogTableInfo)
        self.label_2.setObjectName("label_2")
        self.gridLayout_3.addWidget(self.label_2, 0, 2, 1, 1)
        self.gridLayout.addLayout(self.gridLayout_3, 1, 0, 1, 1)
        self.gridLayout_4 = QtWidgets.QGridLayout()
        self.gridLayout_4.setObjectName("gridLayout_4")
        self.label_3 = QtWidgets.QLabel(DialogTableInfo)
        self.label_3.setObjectName("label_3")
        self.gridLayout_4.addWidget(self.label_3, 2, 0, 1, 1)
        self.label_4 = QtWidgets.QLabel(DialogTableInfo)
        self.label_4.setObjectName("label_4")
        self.gridLayout_4.addWidget(self.label_4, 3, 0, 1, 1)
        self.infoFile_path = QtWidgets.QLineEdit(DialogTableInfo)
        self.infoFile_path.setReadOnly(True)
        self.infoFile_path.setObjectName("infoFile_path")
        self.gridLayout_4.addWidget(self.infoFile_path, 3, 1, 1, 1)
        self.infoFile_name = QtWidgets.QLineEdit(DialogTableInfo)
        self.infoFile_name.setReadOnly(True)
        self.infoFile_name.setObjectName("infoFile_name")
        self.gridLayout_4.addWidget(self.infoFile_name, 2, 1, 1, 1)
        self.label_5 = QtWidgets.QLabel(DialogTableInfo)
        self.label_5.setObjectName("label_5")
        self.gridLayout_4.addWidget(self.label_5, 4, 0, 1, 1)
        self.infoSeverity = QtWidgets.QLabel(DialogTableInfo)
        self.infoSeverity.setStyleSheet("font-weight: bold;\n"
"text-transform: uppercase;")
        self.infoSeverity.setText("")
        self.infoSeverity.setObjectName("infoSeverity")
        self.gridLayout_4.addWidget(self.infoSeverity, 4, 1, 1, 1)
        self.gridLayout.addLayout(self.gridLayout_4, 0, 0, 1, 1)
        self.gridLayout_2.addLayout(self.gridLayout, 1, 0, 1, 1)
        self.horizontalLayout = QtWidgets.QHBoxLayout()
        self.horizontalLayout.setObjectName("horizontalLayout")
        spacerItem1 = QtWidgets.QSpacerItem(40, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.horizontalLayout.addItem(spacerItem1)
        self.buttonOpen_file = QtWidgets.QPushButton(DialogTableInfo)
        self.buttonOpen_file.setObjectName("buttonOpen_file")
        self.horizontalLayout.addWidget(self.buttonOpen_file)
        self.buttonClose = QtWidgets.QPushButton(DialogTableInfo)
        self.buttonClose.setObjectName("buttonClose")
        self.horizontalLayout.addWidget(self.buttonClose)
        self.gridLayout_2.addLayout(self.horizontalLayout, 2, 0, 1, 1)

        self.retranslateUi(DialogTableInfo)
        self.buttonClose.clicked.connect(DialogTableInfo.hide)
        QtCore.QMetaObject.connectSlotsByName(DialogTableInfo)

    def retranslateUi(self, DialogTableInfo):
        _translate = QtCore.QCoreApplication.translate
        DialogTableInfo.setWindowTitle(_translate("DialogTableInfo", "Information"))
        self.label.setText(_translate("DialogTableInfo", "Description"))
        self.label_2.setText(_translate("DialogTableInfo", "Suggestions"))
        self.label_3.setText(_translate("DialogTableInfo", "Name"))
        self.label_4.setText(_translate("DialogTableInfo", "File path"))
        self.label_5.setText(_translate("DialogTableInfo", "Severity"))
        self.buttonOpen_file.setText(_translate("DialogTableInfo", "Open file"))
        self.buttonClose.setText(_translate("DialogTableInfo", "Close"))

