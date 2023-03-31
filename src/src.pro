TARGET = nemocalendar
PLUGIN_IMPORT_PATH = org/nemomobile/calendar

TEMPLATE = lib
CONFIG += qt plugin hide_symbols timed-qt5

QT += qml concurrent
QT -= gui
QMAKE_CXXFLAGS += -Werror

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
PKGCONFIG += KF5CalendarCore libmkcal-qt5 accounts-qt5

INSTALLS += target

qmldir.files += qmldir plugins.qmltypes
qmldir.path +=  $$target.path
INSTALLS += qmldir

qmltypes.commands = qmlplugindump -noinstantiate -nonrelocatable org.nemomobile.calendar 1.0 > $$PWD/plugins.qmltypes
QMAKE_EXTRA_TARGETS += qmltypes

CONFIG += link_pkgconfig

isEmpty(SRCDIR) SRCDIR = "."

SOURCES += \
    $$SRCDIR/plugin.cpp \
    $$SRCDIR/calendarevent.cpp \
    $$SRCDIR/calendareventoccurrence.cpp \
    $$SRCDIR/calendaragendamodel.cpp \
    $$SRCDIR/calendareventlistmodel.cpp \
    $$SRCDIR/calendarsearchmodel.cpp \
    $$SRCDIR/calendarapi.cpp \
    $$SRCDIR/calendareventquery.cpp \
    $$SRCDIR/calendarinvitationquery.cpp \
    $$SRCDIR/calendarnotebookmodel.cpp \
    $$SRCDIR/calendarmanager.cpp \
    $$SRCDIR/calendarworker.cpp \
    $$SRCDIR/calendarnotebookquery.cpp \
    $$SRCDIR/calendareventmodification.cpp \
    $$SRCDIR/calendarutils.cpp \
    $$SRCDIR/calendarimportmodel.cpp \
    $$SRCDIR/calendarimportevent.cpp \
    $$SRCDIR/calendarcontactmodel.cpp \
    $$SRCDIR/calendarattendeemodel.cpp

HEADERS += \
    $$SRCDIR/calendarevent.h \
    $$SRCDIR/calendareventoccurrence.h \
    $$SRCDIR/calendaragendamodel.h \
    $$SRCDIR/calendareventlistmodel.h \
    $$SRCDIR/calendarsearchmodel.h \
    $$SRCDIR/calendarapi.h \
    $$SRCDIR/calendareventquery.h \
    $$SRCDIR/calendarinvitationquery.h \
    $$SRCDIR/calendarnotebookmodel.h \
    $$SRCDIR/calendarmanager.h \
    $$SRCDIR/calendarworker.h \
    $$SRCDIR/calendardata.h \
    $$SRCDIR/calendarnotebookquery.h \
    $$SRCDIR/calendareventmodification.h \
    $$SRCDIR/calendarutils.h \
    $$SRCDIR/calendarimportmodel.h \
    $$SRCDIR/calendarimportevent.h \
    $$SRCDIR/calendarcontactmodel.h \
    $$SRCDIR/calendarattendeemodel.h

OTHER_FILES += qmldir

MOC_DIR = $$PWD/.moc
OBJECTS_DIR = $$PWD/.obj
