cmake_minimum_required(VERSION 3.10.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

project(MavlinkTagController)

if(NOT MSVC)
    add_definitions("-Wall -Wextra")
else()
    add_definitions("-WX -W2")
endif()

find_package(MAVSDK REQUIRED)

add_executable(MavlinkTagController
    main.cpp
    CommandHandler.cpp CommandHandler.h
    UDPPulseReceiver.cpp UDPPulseReceiver.h
    startAirspyProcess.cpp startAirspyProcess.h
    sendTunnelMessage.cpp sendTunnelMessage.h
    TunnelProtocol.h
)

target_link_libraries(MavlinkTagController
    MAVSDK::mavsdk
)
