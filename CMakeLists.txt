cmake_minimum_required(VERSION 3.21)
project(MalachiteVRGameFix VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(MalachiteVRGameFix
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SystemInfo.cpp
    src/SystemInfo.h
    src/Telemetry.cpp
    src/Telemetry.h
    src/Diagnostic.cpp
    src/Diagnostic.h
)

target_link_libraries(MalachiteVRGameFix PRIVATE
    Qt6::Widgets
    dxgi
    pdh
    iphlpapi
    ws2_32
)

target_compile_definitions(MalachiteVRGameFix PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(MalachiteVRGameFix PRIVATE /W4 /permissive-)
else()
    target_compile_options(MalachiteVRGameFix PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(MalachiteVRGameFix PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "malachite-gamefix"
)
