TEMPLATE=app
TARGET=icalconverter
QT-=gui
CONFIG += link_pkgconfig
PKGCONFIG += KF$${QT_MAJOR_VERSION}CalendarCore libmkcal-qt$${QT_MAJOR_VERSION}
QMAKE_CXXFLAGS += -fPIE -fvisibility=hidden -fvisibility-inlines-hidden
SOURCES+=main.cpp

target.path = $$INSTALL_ROOT/usr/bin/
INSTALLS+=target
