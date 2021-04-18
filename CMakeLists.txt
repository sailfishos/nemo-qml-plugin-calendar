cmake_minimum_required(VERSION 3.16.0)

project(nemo-qml-plugin-calendar)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(FindPkgConfig)
include(FeatureSummary)
include(GNUInstallDirs)
include(CTest)

set(INSTALL_QML_IMPORT_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/qt/qml"
	CACHE PATH "Custom QML import installation directory")

find_package(PkgConfig REQUIRED)
find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(QT_MIN_VERSION "5.6.0")
find_package(Qt5 ${QT_MIN_VERSION} COMPONENTS DBus Gui Qml Concurrent Test REQUIRED)
find_package(KF5 COMPONENTS CalendarCore REQUIRED)

pkg_check_modules(MKCAL libmkcal-qt5 IMPORTED_TARGET REQUIRED)
set_property(GLOBAL APPEND PROPERTY _CMAKE_libmkcal-qt5_TYPE REQUIRED)
pkg_check_modules(ACCOUNTS accounts-qt5 IMPORTED_TARGET REQUIRED)
set_property(GLOBAL APPEND PROPERTY _CMAKE_accounts-qt5_TYPE REQUIRED)

if(MKCAL_FOUND)
	set_property(GLOBAL APPEND PROPERTY PACKAGES_FOUND libmkcal-qt5)
else()
	set_property(GLOBAL APPEND PROPERTY PACKAGES_NOT_FOUND libmkcal-qt5)
endif()
if(ACCOUNTS_FOUND)
	set_property(GLOBAL APPEND PROPERTY PACKAGES_FOUND accounts-qt5)
else()
	set_property(GLOBAL APPEND PROPERTY PACKAGES_NOT_FOUND accounts-qt5)
endif()

add_subdirectory(src)
add_subdirectory(lightweight)
add_subdirectory(tools)

if(BUILD_TESTING)
	add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
