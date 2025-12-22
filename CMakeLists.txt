cmake_minimum_required(VERSION 3.16)

project(QSimpleConsole VERSION 1.0.0 LANGUAGES CXX)

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

# Define the static library
add_library(qsimpleconsole STATIC
    src/qsimpleconsole.cpp
    include/qsimpleconsole.h
)

# Specify include directories
target_include_directories(qsimpleconsole
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

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
