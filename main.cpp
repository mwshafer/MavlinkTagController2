#include "CommandHandler.h"
#include "UDPPulseReceiver.h"
#include "TunnelProtocol.h"
#include "sendTunnelMessage.h"
#include "sendStatusText.h"
#include "log.h"
#include "MavlinkOutgoingMessageQueue.h"
#include "startHeartbeatSender.h"
#include "TelemetryCache.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <future>
#include <memory>
#include <thread>

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/ftp/ftp.h>

using namespace mavsdk;

int main(int argc, char** argv)
{
    // Check that TunnelProtocol hasn't exceed limits
    static_assert(TunnelProtocolValidateSizes, "TunnelProtocolValidateSizes failed");

    Mavsdk mavsdk;
    mavsdk.set_configuration(Mavsdk::Configuration(1, MAV_COMP_ID_ONBOARD_COMPUTER, true));

    const char* connectionUrl = "udp://0.0.0.0:14540"; // Default to SITL
    if (argc == 2) {
        connectionUrl = argv[1];
    }
    logInfo() << "Connecting to" << connectionUrl;

    ConnectionResult connection_result;
    connection_result = mavsdk.add_any_connection(connectionUrl);
    if (connection_result != ConnectionResult::Success) {
        logError() << "Connection failed for" << connectionUrl << ":" << connection_result;
        return 1;
    }

    // We need to wait for both the Autopilot and QGC Systems to become availaable

    bool foundAutopilot = false;
    bool foundQGC       = false;

    std::shared_ptr<System> autopilotSystem;
    std::shared_ptr<System> qgcSystem;

    logInfo() << "Waiting to discover Autopilot and QGC";
    while (!foundAutopilot || !foundQGC) {
        std::vector< std::shared_ptr< System > > systems = mavsdk.systems();
        for (size_t i=0; i<systems.size(); i++) {
            std::shared_ptr< System > system = systems[i];
            std::vector< uint8_t > compIds = system->component_ids();
            for (size_t i=0; i < compIds.size(); i++) {
                auto compId = compIds[i];
                if (!foundAutopilot && compId == MAV_COMP_ID_AUTOPILOT1) {
                    logInfo() << "Discovered Autopilot";
                    autopilotSystem = system;
                    foundAutopilot  = true;
                } else if (!foundQGC && compId == MAV_COMP_ID_MISSIONPLANNER && system->get_system_id() == 255) {
                    logInfo() << "Discovered QGC";
                    qgcSystem = system;
                    foundQGC = true;
                } 
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    auto mavlinkPassthrough     = MavlinkPassthrough            { qgcSystem };
    auto outgoingMessageQueue   = MavlinkOutgoingMessageQueue   { mavlinkPassthrough };
    auto telemetryCache         = TelemetryCache                { *autopilotSystem };
    auto udpPulseReceiver       = UDPPulseReceiver              { std::string("127.0.0.1"), 50000, outgoingMessageQueue, telemetryCache };
    auto ftpServer              = Ftp                           { autopilotSystem };
    auto commandHandler         = CommandHandler                { *qgcSystem, outgoingMessageQueue };

    ftpServer.set_root_directory(getenv("HOME"));

    udpPulseReceiver.start();
    startHeartbeatSender(outgoingMessageQueue, 5000);

    logInfo() << "Ready";
    sendStatusText(outgoingMessageQueue, "MavlinkTagController Ready");

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
