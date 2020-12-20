TEMPLATE=app
TARGET=icalconverter
QT-=gui
CONFIG += link_pkgconfig
PKGCONFIG += KF5CalendarCore libmkcal-qt5
QMAKE_CXXFLAGS += -fPIE -fvisibility=hidden -fvisibility-inlines-hidden
SOURCES+=main.cpp

target.path = $$INSTALL_ROOT/usr/bin/
INSTALLS+=target
