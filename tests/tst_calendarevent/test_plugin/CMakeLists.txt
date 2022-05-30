set(SRC test_plugin.cpp)

add_library(tst_calendarevent_plugin ${SRC})

target_include_directories(tst_calendarevent_plugin PRIVATE
	${PROJECT_SOURCE_DIR}/src
	${TARGET_MOC_DIR})

target_link_libraries(tst_calendarevent_plugin
	Qt5::Qml
	Qt5::Test
	KF5::CalendarCore
	PkgConfig::MKCAL)
