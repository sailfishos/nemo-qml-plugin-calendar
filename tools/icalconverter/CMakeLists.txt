set(SRC main.cpp)

add_executable(icalconverter ${SRC})

target_link_libraries(icalconverter
	KF5::CalendarCore
	PkgConfig::MKCAL)

install(TARGETS icalconverter
	DESTINATION ${CMAKE_INSTALL_BINDIR})
