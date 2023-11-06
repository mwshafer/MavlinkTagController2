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
	PulseSimulator(MavlinkSystem* mavlink, uint32_t antennaOffset);

private:
	double _snrFromYaw	(double vehicleYawDegrees);
	double _normalizeYaw(double yaw);

	MavlinkSystem* 	_mavlink {}; 
	uint32_t 		_antennaOffset {};
};
