#include "log.h"
#include "CommandHandler.h"
#include "UDPPulseReceiver.h"
#include "TunnelProtocol.h"
//#include "TelemetryCache.h"
#include "MavlinkSystem.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <future>
#include <memory>
#include <thread>

int main(int argc, char** argv)
{
	setbuf(stdout, NULL); // Disable stdout buffering

    // Check that TunnelProtocol hasn't exceed limits
    static_assert(TunnelProtocolValidateSizes, "TunnelProtocolValidateSizes failed");

	std::string connectionUrl = "udp://127.0.0.1:14540";    // default to SITL
    if (argc == 2) {
        connectionUrl = argv[1];
    }
    logInfo() << "Connecting to" << connectionUrl;

	auto mavlink 			= new MavlinkSystem(connectionUrl);
    auto commandHandler 	= CommandHandler { mavlink };
    auto udpPulseReceiver   = UDPPulseReceiver { std::string("127.0.0.1"), 50000, mavlink };

    udpPulseReceiver.start();

	if (!mavlink->start()) {
		logError() << "Mavlink start failed";
		return 1;
	}

	logInfo() << "Waiting for autopilot heartbeat...";
	while (!mavlink->connected()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	bool tunnelHeartbeatsStarted = false;
	while (true) {
		if (!tunnelHeartbeatsStarted && mavlink->gcsSystemId().has_value()) {
			tunnelHeartbeatsStarted = true;
			mavlink->startTunnelHeartbeatSender();
		    mavlink->sendStatusText("MavlinkTagController Ready");
		}

		// Do nothing -- message subscription callbacks are asynchronous and run in the connection receiver thread
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	logInfo() << "Exiting...";

#if 0
    auto mavlinkPassthrough     = MavlinkPassthrough            { qgcSystem };
    auto outgoingMessageQueue   = MavlinkOutgoingMessageQueue   { mavlinkPassthrough };
    auto telemetryCache         = TelemetryCache                { mavlink };
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
#endif

    return 0;
}
