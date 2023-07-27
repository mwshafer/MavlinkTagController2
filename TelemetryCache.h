#pragma once

#include "MavlinkSystem.h"

#include <chrono>
#include <list>
#include <mutex>

class TelemetryCache
{
public:
	typedef struct {
		double 	latitude;
		double 	longitude;
		double 	altitude;
	} Position_t;

	typedef struct {
		float rollDegrees;
		float pitchDegrees;
		float yawDegrees;
	} EulerAngle_t;

	typedef struct {
		float w;
		float x;
		float y;
		float z;
	} Quaternion_t;

	typedef struct {
		double			timeInSeconds;
		Position_t 		position;
		Quaternion_t 	attitudeQuaternion;
		EulerAngle_t 	attitudeEuler;
	} TelemetryCacheEntry_t;

	TelemetryCache(std::shared_ptr<MavlinkSystem> mavlink);
	~TelemetryCache();

	TelemetryCacheEntry_t telemetryForTime(double timeInSeconds);

private:
	void _positionCallback			(const mavlink_message_t& message);
	void _attitudeQuaternionCallback(const mavlink_message_t& message);
	void _attitudeEulerCallback		(const mavlink_message_t& message);

	std::shared_ptr<MavlinkSystem> 		_mavlink;
	Position_t 							_lastPosition;
	Quaternion_t 						_lastAttitudeQuaternion;
	EulerAngle_t 						_lastAttitudeEuler;
	bool								_havePosition			= false;
	bool								_haveAttitudeQuaternion	= false;
	bool								_haveAttitudeEuler		= false;
	std::list<TelemetryCacheEntry_t> 	_telemetryCache;
	std::mutex							_telemetryCacheMutex;
};