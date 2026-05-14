TEMPLATE = app
TARGET = calendardataservice
target.path = /usr/bin

QT += qml dbus
QT -= gui

CONFIG += link_pkgconfig
PKGCONFIG += KF$${QT_MAJOR_VERSION}CalendarCore libmkcal-qt$${QT_MAJOR_VERSION} accounts-qt$${QT_MAJOR_VERSION} timed-qt$${QT_MAJOR_VERSION}

HEADERS += \
    calendardataservice.h \
    calendardataserviceadaptor.h \
    ../common/eventdata.h \
    ../../src/calendaragendamodel.h \
    ../../src/calendareventlistmodel.h \
    ../../src/calendarsearchmodel.h \
    ../../src/calendarmanager.h \
    ../../src/calendarworker.h \
    ../../src/calendareventoccurrence.h \
    ../../src/calendarevent.h \
    ../../src/calendareventquery.h \
    ../../src/calendarinvitationquery.h \
    ../../src/calendarutils.h

SOURCES += \
    calendardataservice.cpp \
    calendardataserviceadaptor.cpp \
    ../common/eventdata.cpp \
    ../../src/calendaragendamodel.cpp \
    ../../src/calendareventlistmodel.cpp \
    ../../src/calendarsearchmodel.cpp \
    ../../src/calendarmanager.cpp \
    ../../src/calendarworker.cpp \
    ../../src/calendareventoccurrence.cpp \
    ../../src/calendarevent.cpp \
    ../../src/calendareventquery.cpp \
    ../../src/calendarinvitationquery.cpp \
    ../../src/calendarutils.cpp \
    main.cpp

dbus_service.path = /usr/share/dbus-1/services/
dbus_service.files = org.nemomobile.calendardataservice.service

INSTALLS += target dbus_service

OTHER_FILES += *.service *.xml
