set(SRC tst_calendarevent.cpp)

add_executable(tst_calendarevent ${SRC})

target_include_directories(tst_calendarevent PRIVATE
	${PROJECT_SOURCE_DIR}/src
	${TARGET_MOC_DIR})

target_link_libraries(tst_calendarevent
	Qt5::Qml
	Qt5::Test
	KF5::CalendarCore
	nemocalendar)

add_test(tst_calendarevent tst_calendarevent)

add_subdirectory(test_plugin)
