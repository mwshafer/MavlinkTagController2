#pragma once

#include "MavlinkOutgoingMessageQueue.h"

void startHeartbeatSender(MavlinkOutgoingMessageQueue& outgoingMessageQueue, int heartbeatIntervalMs);