cmake_minimum_required(VERSION 3.16)

project(minimal LANGUAGES CXX)

# mainwindow.h declares Q_OBJECT, so moc has to run. There are no .ui or .qrc
# files, so AUTOUIC and AUTORCC are left off.
set(CMAKE_AUTOMOC ON)

# Build the library alongside the example. The guard lets this file work both
# standalone and when the example is added from the top-level project.
if(NOT TARGET qsimpleconsole)
    add_subdirectory(../.. qsimpleconsole_build)
endif()

add_executable(minimal
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

# The library links Qt Widgets and requires C++17 publicly, so neither needs
# repeating here. This is also the target spelling the README documents.
target_link_libraries(minimal PRIVATE QSimpleConsole::qsimpleconsole)

set_target_properties(minimal PROPERTIES WIN32_EXECUTABLE TRUE)
