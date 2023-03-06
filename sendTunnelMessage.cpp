#include "sendTunnelMessage.h"

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

void sendTunnelMessage(MavlinkOutgoingMessageQueue& outgoingMessageQueue, void* tunnelPayload, size_t tunnelPayloadSize)
{
    mavlink_message_t   message;
    mavlink_tunnel_t    tunnel;

    mavsdk::MavlinkPassthrough& mavlinkPassthrough = outgoingMessageQueue.mavlinkPassthrough();

    memset(&tunnel, 0, sizeof(tunnel));

    tunnel.target_system    = mavlinkPassthrough.get_target_sysid();
    tunnel.target_component = mavlinkPassthrough.get_target_compid();
    tunnel.payload_type     = MAV_TUNNEL_PAYLOAD_TYPE_UNKNOWN;
    tunnel.payload_length   = tunnelPayloadSize;

    memcpy(tunnel.payload, tunnelPayload, tunnelPayloadSize);

    mavlink_msg_tunnel_encode(
        mavlinkPassthrough.get_our_sysid(),
        mavlinkPassthrough.get_our_compid(),
        &message,
        &tunnel);

    outgoingMessageQueue.addMessage(message);
}