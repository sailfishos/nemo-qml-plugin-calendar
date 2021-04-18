set(PLUGIN_IMPORT_PATH ${INSTALL_QML_IMPORT_DIR}/org/nemomobile/calendar/lightweight)

set(SRC
	calendardataserviceproxy.cpp
	calendareventsmodel.cpp
	plugin.cpp
	../common/eventdata.cpp)
set(HEADERS
	calendardataserviceproxy.h
	calendareventsmodel.h
	../common/eventdata.h)

add_library(calendareventsmodel SHARED ${SRC} ${HEADERS})

target_link_libraries(calendareventsmodel
	Qt5::Gui
	Qt5::Qml
	Qt5::DBus)

install(TARGETS calendareventsmodel
	DESTINATION ${PLUGIN_IMPORT_PATH})
install(FILES qmldir plugins.qmltypes
	DESTINATION ${PLUGIN_IMPORT_PATH})
