set(SRC
	main.cpp
	calendardataservice.cpp
	calendardataserviceadaptor.cpp
	../common/eventdata.cpp)
set(HEADERS
	calendardataservice.h
	calendardataserviceadaptor.h
	../common/eventdata.h)

add_executable(calendardataservice ${SRC} ${HEADERS})
target_link_libraries(calendardataservice
	Qt5::Qml
	Qt5::DBus
	KF5::CalendarCore
	mkcal-qt5
	accounts-qt5
	nemocalendar)

install(TARGETS calendardataservice
	DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES org.nemomobile.calendardataservice.service
	DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
