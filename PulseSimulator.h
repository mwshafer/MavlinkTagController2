#pragma once

#include <mavlink.h>

#include "ThreadSafeQueue.h"
#include "MavlinkSystem.h"

#include <string>
#include <thread>
#include <optional>

class PulseSimulator
{
public:
	PulseSimulator(MavlinkSystem* mavlink);

private:
	double _snrFromYaw(double yawDegrees);

	MavlinkSystem* _mavlink {}; 
};
