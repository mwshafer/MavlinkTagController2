#include "TelemetryCache.h"
#include "MavlinkSystem.h"
#include "log.h"

#include <functional>
#include <thread>
#include <chrono>
#include <cmath>
#include <ctime>
#include <time.h>

TelemetryCache::TelemetryCache(MavlinkSystem* mavlink)
    : _mavlink(mavlink)
{
	_mavlink->subscribeToMessage(MAVLINK_MSG_ID_GLOBAL_POSITION_INT, std::bind(&TelemetryCache::_positionCallback,           this, std::placeholders::_1));
	_mavlink->subscribeToMessage(MAVLINK_MSG_ID_ATTITUDE_QUATERNION, std::bind(&TelemetryCache::_attitudeQuaternionCallback, this, std::placeholders::_1));

    std::thread([this]()
    {
        while (true) {
            if (_havePosition && _haveAttitudeQuaternion && _haveAttitudeEuler) {
                std::lock_guard<std::mutex> lock(_telemetryCacheMutex);
                TelemetryCacheEntry_t       entry;

                auto    timeSince       = std::chrono::system_clock::now().time_since_epoch();
                auto    timeSinceMSecs  = std::chrono::duration_cast<std::chrono::milliseconds>(timeSince).count();
                double  timeSinceSecs   = timeSinceMSecs / 1000.0;

                entry.timeInSeconds         =  timeSinceSecs;
                entry.position              = _lastPosition;
                entry.attitudeQuaternion    = _lastAttitudeQuaternion;
                entry.attitudeEuler         = _lastAttitudeEuler;
                _telemetryCache.push_back(entry);
                _pruneTelemetryCache();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }).detach();
}

TelemetryCache::~TelemetryCache()
{
}

void TelemetryCache::_positionCallback(const mavlink_message_t& message)
{
    mavlink_global_position_int_t globalPositionInt;
    mavlink_msg_global_position_int_decode(&message, &globalPositionInt);

    // ArduPilot sends bogus GLOBAL_POSITION_INT messages with lat/lat 0/0 even when it has no gps signal
    // Apparently, this is in order to transport relative altitude information.
    if (globalPositionInt.lat == 0 && globalPositionInt.lon == 0) {
        return;
    }

    _lastPosition.latitude          = globalPositionInt.lat  / (double)1E7;
    _lastPosition.longitude         = globalPositionInt.lon  / (double)1E7;
    _lastPosition.relativeAltitude  = globalPositionInt.relative_alt * 1e-3f;

    _havePosition = true;
}

float TelemetryCache::_toDegFromRad(float rad)
{
    return 180.0f / static_cast<float>(M_PI) * rad;
}

TelemetryCache::EulerAngle_t TelemetryCache::_toEulerAngleFromQuaternion(TelemetryCache::Quaternion_t quaternion)
{
    auto& q = quaternion;

    EulerAngle_t eulerAngle;
    eulerAngle.rollDegrees = _toDegFromRad(
        atan2f(2.0f * (q.w * q.x + q.y * q.z), 1.0f - 2.0f * (q.x * q.x + q.y * q.y)));
    eulerAngle.pitchDegrees = _toDegFromRad(asinf(2.0f * (q.w * q.y - q.z * q.x)));
    eulerAngle.yawDegrees = _toDegFromRad(
        atan2f(2.0f * (q.w * q.z + q.x * q.y), 1.0f - 2.0f * (q.y * q.y + q.z * q.z)));

    return eulerAngle;
}

void TelemetryCache::_attitudeQuaternionCallback(const mavlink_message_t& message)
{
    // only accept the attitude message from the vehicle's flight controller
    if (message.sysid != _mavlink->ourSystemId() || message.compid != MAV_COMP_ID_AUTOPILOT1) {
        return;
    }

    mavlink_attitude_quaternion_t attitudeQuaternion;
    mavlink_msg_attitude_quaternion_decode(&message, &attitudeQuaternion);

    _lastAttitudeQuaternion.w = attitudeQuaternion.q1;
    _lastAttitudeQuaternion.x = attitudeQuaternion.q2;
    _lastAttitudeQuaternion.y = attitudeQuaternion.q3;
    _lastAttitudeQuaternion.z = attitudeQuaternion.q4;

    _lastAttitudeEuler = _toEulerAngleFromQuaternion(_lastAttitudeQuaternion);

    _haveAttitudeQuaternion = true;
    _haveAttitudeEuler      = true;
}

TelemetryCache::TelemetryCacheEntry_t TelemetryCache::telemetryForTime(double timeInSeconds)
{
    std::lock_guard<std::mutex>     lock            (_telemetryCacheMutex);
    TelemetryCacheEntry_t           bestEntry       { };
    double                          bestDiffMSecs   = -1;

    // Hack to convert matlab local time to gmt
    //timeInSeconds += 7 * 60 * 60;

    // Find the closest matching entry in the cache
    for (auto entry : _telemetryCache) {

        double  entryDiffMSecs  = std::fabs(timeInSeconds - entry.timeInSeconds);

        if (bestDiffMSecs < 0) {
            bestDiffMSecs = entryDiffMSecs;
        } else {
            if (entryDiffMSecs < bestDiffMSecs) {
                bestDiffMSecs = entryDiffMSecs;
                bestEntry = entry;
            }
        }
    }

    return bestEntry;
}

void TelemetryCache::_pruneTelemetryCache()
{
    auto    timeSince           = std::chrono::system_clock::now().time_since_epoch();
    auto    nowMSecs            = std::chrono::duration_cast<std::chrono::milliseconds>(timeSince).count();
    double  nowSecs             = nowMSecs / 1000.0;
    double  maxIntraPulseSecs   = 5.0;
    double  maxK                = 3.0;
    double  pruneBeforeSecs     = nowSecs - (((maxK + 1) * maxIntraPulseSecs) * 2);

    while (_telemetryCache.size() > 0) {
        auto& entry = _telemetryCache.front();

        if (entry.timeInSeconds < pruneBeforeSecs) {
            _telemetryCache.pop_front();
        } else {
            break;
        }
    }
}
