#pragma once

#include <chrono>
#include <list>
#include <mutex>

#include <mavlink.h>

class MavlinkSystem;

class TelemetryCache
{
public:
	typedef struct {
		double 	latitude;
		double 	longitude;
		double 	relativeAltitude;
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

	TelemetryCache(MavlinkSystem* mavlink);
	~TelemetryCache();

	TelemetryCacheEntry_t telemetryForTime(double timeInSeconds);

private:
	void 			_positionCallback			(const mavlink_message_t& message);
	void 			_attitudeQuaternionCallback	(const mavlink_message_t& message);
	float 			_toDegFromRad				(float rad);
	EulerAngle_t 	_toEulerAngleFromQuaternion	(Quaternion_t quaternion);

	MavlinkSystem* 						_mavlink;
	Position_t 							_lastPosition;
	Quaternion_t 						_lastAttitudeQuaternion;
	EulerAngle_t 						_lastAttitudeEuler;
	bool								_havePosition			= false;
	bool								_haveAttitudeQuaternion	= false;
	bool								_haveAttitudeEuler		= false;
	std::list<TelemetryCacheEntry_t> 	_telemetryCache;
	std::mutex							_telemetryCacheMutex;
};