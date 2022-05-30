set(SRC tst_calendarmanager.cpp)

add_executable(tst_calendarmanager ${SRC})

target_include_directories(tst_calendarmanager PRIVATE
	${PROJECT_SOURCE_DIR}/src
	${TARGET_MOC_DIR})

target_link_libraries(tst_calendarmanager
	Qt5::Qml
	Qt5::Test
	KF5::CalendarCore
	nemocalendar)

add_test(tst_calendarmanager tst_calendarmanager)
