#pragma once

#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/system.h>

#include <chrono>
#include <list>
#include <mutex>

class TelemetryCache
{
public:
	typedef struct {
		double							timeInSeconds;
		mavsdk::Telemetry::Position 	position;
		mavsdk::Telemetry::Quaternion 	attitudeQuaternion;
		mavsdk::Telemetry::EulerAngle 	attitudeEuler;
	} TelemetryCacheEntry_t;

	TelemetryCache(mavsdk::System& system);
	~TelemetryCache();

	TelemetryCacheEntry_t telemetryForTime(double timeInSeconds);

private:
	void _positionCallback			(mavsdk::Telemetry::Position 	position);
	void _attitudeQuaternionCallback(mavsdk::Telemetry::Quaternion 	attitude);
	void _attitudeEulerCallback		(mavsdk::Telemetry::EulerAngle 	attitude);

	mavsdk::System& 					_system;
	mavsdk::Telemetry 					_telemetry;
	mavsdk::Telemetry::Position 		_lastPosition;
	mavsdk::Telemetry::Quaternion 		_lastAttitudeQuaternion;
	mavsdk::Telemetry::EulerAngle 		_lastAttitudeEuler;
	bool								_havePosition			= false;
	bool								_haveAttitudeQuaternion	= false;
	bool								_haveAttitudeEuler		= false;
	std::list<TelemetryCacheEntry_t> 	_telemetryCache;
	std::mutex							_telemetryCacheMutex;
};