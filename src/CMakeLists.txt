set(PLUGIN_IMPORT_PATH ${INSTALL_QML_IMPORT_DIR}/org/nemomobile/calendar)
set(CMAKE_INSTALL_RPATH "${PLUGIN_IMPORT_PATH}")

set(SRC
	plugin.cpp
	calendarevent.cpp
	calendareventoccurrence.cpp
	calendaragendamodel.cpp
	calendarapi.cpp
	calendareventquery.cpp
	calendarinvitationquery.cpp
	calendarnotebookmodel.cpp
	calendarmanager.cpp
	calendarworker.cpp
	calendarnotebookquery.cpp
	calendareventmodification.cpp	
	calendarutils.cpp
	calendarimportmodel.cpp
	calendarimportevent.cpp
	calendarcontactmodel.cpp
	calendarattendeemodel.cpp
	calendareventlistmodel.cpp)
set(HEADERS
	calendarevent.h
	calendareventoccurrence.h
	calendaragendamodel.h
	calendarapi.h
	calendareventquery.h
	calendarinvitationquery.h
	calendarnotebookmodel.h
	calendarmanager.h
	calendarworker.h
	calendardata.h
	calendarnotebookquery.h
	calendareventmodification.h	
	calendarutils.h
	calendarimportmodel.h
	calendarimportevent.h
	calendarcontactmodel.h
	calendarattendeemodel.h
	calendareventlistmodel.h)

add_library(nemocalendar SHARED ${SRC} ${HEADERS})

set(TARGET_MOC_DIR ${CMAKE_CURRENT_BINARY_DIR}/calendar_autogen)
set_target_properties(nemocalendar PROPERTIES AUTOGEN_BUILD_DIR ${TARGET_MOC_DIR})

# For use in other parts of the build
set(TARGET_MOC_DIR ${TARGET_MOC_DIR}/include PARENT_SCOPE)

target_link_libraries(nemocalendar
	Qt5::Qml
	Qt5::Concurrent
	KF5::CalendarCore
	PkgConfig::MKCAL
	PkgConfig::ACCOUNTS)

# Install the library
install(TARGETS nemocalendar
	LIBRARY DESTINATION ${PLUGIN_IMPORT_PATH})
install(FILES qmldir plugins.qmltypes
	DESTINATION ${PLUGIN_IMPORT_PATH})
