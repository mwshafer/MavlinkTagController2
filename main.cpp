#include "CommandHandler.h"
#include "UDPPulseReceiver.h"
#include "startAirspyProcess.h"

#include <chrono>
#include <cstdint>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <iostream>
#include <future>
#include <memory>
#include <thread>

using namespace mavsdk;

int main(int /*argc*/, char** /*argv[]*/)
{
    Mavsdk mavsdk;
    mavsdk.set_configuration(Mavsdk::Configuration(1, MAV_COMP_ID_ONBOARD_COMPUTER, true));

    ConnectionResult connection_result;
    connection_result = mavsdk.add_any_connection("serial:///dev/ttyS0:1500000");
    if (connection_result != ConnectionResult::Success) {
        std::cout << "Connection failed for ttyS0: " << connection_result << std::endl;
        std::cout << "Connecting to udp instead" << std::endl;
        connection_result = mavsdk.add_any_connection("udp://0.0.0.0:14540");
        if (connection_result != ConnectionResult::Success) {
            std::cout << "Connection failed for udp: " << connection_result << std::endl;
            return 1;
        }
    }


    std::cout << "Waiting to discover Autopilot\n";

    // Wait 3 seconds for the Autopilot System to show up
    auto autopilotPromise   = std::promise<std::shared_ptr<System>>{};
    auto autopilotFuture    = autopilotPromise.get_future();
    mavsdk.subscribe_on_new_system([&mavsdk, &autopilotPromise]() {
        auto system = mavsdk.systems().back();
        std::vector< uint8_t > compIds = system->component_ids();
        if (std::find(compIds.begin(), compIds.end(), MAV_COMP_ID_AUTOPILOT1) != compIds.end()) {
            std::cout << "Discovered Autopilot" << std::endl;
            autopilotPromise.set_value(system);
            mavsdk.subscribe_on_new_system(nullptr);            
        }
    });
    if (autopilotFuture.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
        std::cerr << "No autopilot found, exiting.\n";
        return 1;
    }

    // Since we can't do anything without the QGC connection we wait for it indefinitely
    auto qgcPromise = std::promise<std::shared_ptr<System>>{};
    auto qgcFuture  = qgcPromise.get_future();
    mavsdk.subscribe_on_new_system([&mavsdk, &qgcPromise]() {
        auto system = mavsdk.systems().back();
        std::vector< uint8_t > compIds = system->component_ids();
        if (std::find(compIds.begin(), compIds.end(), MAV_COMP_ID_MISSIONPLANNER) != compIds.end()) {
            std::cout << "Discovered QGC" << std::endl;
            qgcPromise.set_value(system);
            mavsdk.subscribe_on_new_system(nullptr);            
        }
    });
    qgcFuture.wait();

    // We have both systems ready for use now
    auto autopilotSystem    = autopilotFuture.get();
    auto qgcSystem          = qgcFuture.get();

    auto mavlinkPassthrough = MavlinkPassthrough{ qgcSystem };
    auto udpPulseReceiver   = UDPPulseReceiver{ "127.0.0.1", 30000, mavlinkPassthrough };
    
    CommandHandler{ *qgcSystem, mavlinkPassthrough };

    udpPulseReceiver.start();

    std::cout << "Ready" << std::endl;

    while (true) {
        udpPulseReceiver.receive();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

    return 0;
}
