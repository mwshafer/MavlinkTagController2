#pragma once

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

using namespace mavsdk;

void sendTunnelMessage(MavlinkPassthrough& mavlinkPassthrough, void* tunnelPayload, size_t tunnelPayloadSize);