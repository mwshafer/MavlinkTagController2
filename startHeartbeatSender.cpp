#include "startHeartbeatSender.h"
#include "TunnelProtocol.h"
#include "sendTunnelMessage.h"

void startHeartbeatSender(MavlinkOutgoingMessageQueue& outgoingMessageQueue, int heartbeatIntervalMs)
{
    std::thread heartbeatSenderThread([&outgoingMessageQueue, heartbeatIntervalMs]() {
        while (true) {
            TunnelProtocol::Heartbeat_t heartbeat;

            heartbeat.header.command    = COMMAND_ID_HEARTBEAT;
            heartbeat.system_id         = HEARTBEAT_SYSTEM_MAVLINKCONTROLLER;

            sendTunnelMessage(outgoingMessageQueue, &heartbeat, sizeof(heartbeat));

            std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatIntervalMs));
        }
    });
    heartbeatSenderThread.detach();
}