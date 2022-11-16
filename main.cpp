#include "CommandHandler.h"
#include "UDPPulseReceiver.h"
#include "startAirspyProcess.h"
#include "TunnelProtocol.h"
#include "sendTunnelMessage.h"
#include "sendStatusText.h"

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

int main(int argc, char** argv)
{
    // Check that TunnelProtocol hasn't exceed limits
    static_assert(TunnelProtocolValidateSizes, "TunnelProtocolValidateSizes failed");

    Mavsdk mavsdk;
    mavsdk.set_configuration(Mavsdk::Configuration(1, MAV_COMP_ID_ONBOARD_COMPUTER, true));

    if (argc != 2) {
        std::cout << "Connection url must be specified on command line" << std::endl;
        return 1;
    }

    ConnectionResult connection_result;
    connection_result = mavsdk.add_any_connection(argv[1]);
    if (connection_result != ConnectionResult::Success) {
        std::cout << "Connection failed for " << argv[1] << ": " << connection_result << std::endl;
        return 1;
    }

    // We need to wait for both the Autopilot and QGC Systems to become availaable

    bool foundAutopilot = false;
    bool foundQGC       = false;

    std::shared_ptr<System> autopilotSystem;
    std::shared_ptr<System> qgcSystem;

    std::cout << "Waiting to discover Autopilot and QGC" << std::endl;
    while (!foundAutopilot || !foundQGC) {
        std::vector< std::shared_ptr< System > > systems = mavsdk.systems();
        for (size_t i=0; i<systems.size(); i++) {
            std::shared_ptr< System > system = systems[i];
            std::vector< uint8_t > compIds = system->component_ids();
            for (size_t i=0; i < compIds.size(); i++) {
                auto compId = compIds[i];
                if (!foundAutopilot && compId == MAV_COMP_ID_AUTOPILOT1) {
                    std::cout << "Discovered Autopilot" << std::endl;
                    autopilotSystem = system;
                    foundAutopilot  = true;
                } else if (!foundQGC && compId == MAV_COMP_ID_MISSIONPLANNER && system->get_system_id() == 255) {
                    std::cout << "Discovered QGC" << std::endl;
                    qgcSystem = system;
                    foundQGC = true;
                } 
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    auto mavlinkPassthrough = MavlinkPassthrough{ qgcSystem };
    auto udpPulseReceiver   = UDPPulseReceiver{ "127.0.0.1", 30000, mavlinkPassthrough };
    
    auto commandHandler = CommandHandler{ *qgcSystem, mavlinkPassthrough };

    udpPulseReceiver.start();

    std::cout << "Ready" << std::endl;
    sendStatusText(mavlinkPassthrough, "MavlinkTagController Ready");

    while (true) {
        udpPulseReceiver.receive();
        commandHandler.process();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

    return 0;
}
