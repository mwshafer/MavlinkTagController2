#pragma once

#include "MavlinkOutgoingMessageQueue.h"

void sendStatusText(MavlinkOutgoingMessageQueue& outgoingMessageQueue, const char* text, uint8_t severity = MAV_SEVERITY_INFO);