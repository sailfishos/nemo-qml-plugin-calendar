set(SRC tst_calendaragendamodel.cpp)

add_executable(tst_calendaragendamodel ${SRC})

target_include_directories(tst_calendaragendamodel PRIVATE
	${PROJECT_SOURCE_DIR}/src
	${TARGET_MOC_DIR})

target_link_libraries(tst_calendaragendamodel
	Qt5::Qml
	Qt5::Test
	KF5::CalendarCore
	nemocalendar)

add_test(tst_calendaragendamodel tst_calendaragendamodel)
