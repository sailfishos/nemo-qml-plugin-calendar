SRCDIR = ../../src/
include(../src/src.pro)
INCLUDEPATH += $$SRCDIR
DEPENDPATH = $$INCLUDEPATH
QT += testlib
TEMPLATE = app
CONFIG -= app_bundle

target.path = /opt/tests/nemo-qml-plugin-calendar-qt5
