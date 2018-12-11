TARGET = nemocalendarwidget
PLUGIN_IMPORT_PATH = org/nemomobile/calendar/lightweight

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

QT += qml dbus
QT -= gui

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += qmldir plugins.qmltypes
qmldir.path +=  $$target.path
INSTALLS += qmldir

qmltypes.commands = qmlplugindump -nonrelocatable org.nemomobile.calendar.lightweight 1.0 > $$PWD/plugins.qmltypes
QMAKE_EXTRA_TARGETS += qmltypes

isEmpty(SRCDIR) SRCDIR = "."
SOURCES += \
    calendardataserviceproxy.cpp \
    calendareventsmodel.cpp \
    ../common/eventdata.cpp \
    plugin.cpp

HEADERS += \
    calendardataserviceproxy.h \
    calendareventsmodel.h \
    ../common/eventdata.h

OTHER_FILES += qmldir

MOC_DIR = $$PWD/.moc
OBJECTS_DIR = $$PWD/.obj
