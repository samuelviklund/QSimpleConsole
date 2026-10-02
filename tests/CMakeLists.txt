find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

add_executable(tst_qsimpleconsole
    tst_qsimpleconsole.cpp
)

target_link_libraries(tst_qsimpleconsole
    PRIVATE
        qsimpleconsole
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME qsimpleconsole COMMAND tst_qsimpleconsole)

# The tests drive a real widget, so they need a platform plugin but not a
# display server.
set_tests_properties(qsimpleconsole PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
