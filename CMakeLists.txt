cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

project(MavlinkTagController)

add_executable(MavlinkTagController
    main.cpp
    CommandHandler.cpp
    UDPPulseReceiver.cpp
)

find_package(MAVSDK REQUIRED)

target_link_libraries(MavlinkTagController
    MAVSDK::mavsdk
)

if(NOT MSVC)
    add_compile_options(MavlinkTagController PRIVATE -Wall -Wextra)
else()
    add_compile_options(MavlinkTagController PRIVATE -WX -W2)
endif()
