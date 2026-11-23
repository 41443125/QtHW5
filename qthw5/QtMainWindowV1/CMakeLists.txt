cmake_minimum_required(VERSION 3.21)
project(QtMainWindowV1 LANGUAGES CXX)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Concurrent)

qt_add_executable(QtMainWindowV1 WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    imagefile.cpp
    imagefile.h
)
target_compile_features(QtMainWindowV1 PRIVATE cxx_std_17)
target_link_libraries(QtMainWindowV1 PRIVATE Qt6::Widgets Qt6::Concurrent)
if(MSVC)
    target_compile_options(QtMainWindowV1 PRIVATE /utf-8)
endif()

option(BUILD_TESTING "Build the image file regression tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    qt_add_executable(ImageFileTests tests/imagefile_tests.cpp imagefile.cpp)
    set_target_properties(ImageFileTests PROPERTIES AUTOMOC ON)
    target_include_directories(ImageFileTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(ImageFileTests PRIVATE cxx_std_17)
    target_link_libraries(ImageFileTests PRIVATE Qt6::Gui Qt6::Concurrent Qt6::Test)
    if(MSVC)
        target_compile_options(ImageFileTests PRIVATE /utf-8)
    endif()
    add_test(NAME ImageFileTests COMMAND ImageFileTests)
endif()
