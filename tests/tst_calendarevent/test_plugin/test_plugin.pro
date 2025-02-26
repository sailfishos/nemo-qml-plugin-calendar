TARGET = testplugin

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

QT -= gui

target.path = /opt/tests/nemo-qml-plugin-calendar-qt5/plugins
PKGCONFIG += KF5CalendarCore libmkcal-qt5

INSTALLS += target

CONFIG += link_pkgconfig

SOURCES += test_plugin.cpp
HEADERS += test_plugin.h
