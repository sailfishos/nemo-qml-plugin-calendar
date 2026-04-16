TARGET = testplugin

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

QT -= gui

target.path = /opt/tests/nemo-qml-plugin-calendar-qt$${QT_MAJOR_VERSION}/plugins
PKGCONFIG += KF$${QT_MAJOR_VERSION}CalendarCore libmkcal-qt$${QT_MAJOR_VERSION}

INSTALLS += target

CONFIG += link_pkgconfig

SOURCES += test_plugin.cpp
HEADERS += test_plugin.h
