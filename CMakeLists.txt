cmake_minimum_required(VERSION 3.16)

project(QSimpleConsole VERSION 1.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt (supports both Qt5 and Qt6)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

include(GNUInstallDirs)

# Define the static library
add_library(qsimpleconsole STATIC
    src/qsimpleconsole.cpp
    include/qsimpleconsole.h
)

# Specify include directories
target_include_directories(qsimpleconsole
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# So that add_subdirectory() and find_package() consumers spell it the same way
add_library(QSimpleConsole::qsimpleconsole ALIAS qsimpleconsole)

# Link against Qt
target_link_libraries(qsimpleconsole
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Widgets
)

# Set library output properties
set_target_properties(qsimpleconsole PROPERTIES
    OUTPUT_NAME "qsimpleconsole"
    DEBUG_POSTFIX "d"
)

# Optional: Set specific output directory
set_target_properties(qsimpleconsole PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release"
)

# Installation
install(TARGETS qsimpleconsole
    EXPORT QSimpleConsoleTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES include/qsimpleconsole.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT QSimpleConsoleTargets
    FILE QSimpleConsoleTargets.cmake
    NAMESPACE QSimpleConsole::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QSimpleConsole
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/QSimpleConsoleConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/QSimpleConsoleConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QSimpleConsole
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/QSimpleConsoleConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/QSimpleConsoleConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/QSimpleConsoleConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/QSimpleConsole
)

# Tests
option(QSIMPLECONSOLE_BUILD_TESTS "Build the QSimpleConsole tests" OFF)
if(QSIMPLECONSOLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
