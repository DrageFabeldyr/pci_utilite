QT += core
QT -= gui

CONFIG += c++11

TARGET = pci_utilite
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    my_driver.cpp

HEADERS += \
    my_driver.h \
    mycommandlist.h
