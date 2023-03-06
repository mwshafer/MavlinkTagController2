#pragma once

#include "MavlinkOutgoingMessageQueue.h"

void sendTunnelMessage(MavlinkOutgoingMessageQueue& outgoingMessageQueue, void* tunnelPayload, size_t tunnelPayloadSize);