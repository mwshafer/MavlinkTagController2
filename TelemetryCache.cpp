#include "TelemetryCache.h"

#include <functional>
#include <thread>
#include <chrono>

TelemetryCache::TelemetryCache(std::shared_ptr<MavlinkSystem> mavlink)
    : _mavlink(mavlink)
{
	_mavlink->subscribe_to_message(MAVLINK_MSG_ID_GLOBAL_POSITION_INT,  std::bind(&TelemetryCache::_positionCallback,           this, std::placeholders::_1));
	_mavlink->subscribe_to_message(MAVLINK_MSG_ID_ATTITUDE_QUATERNION,  std::bind(&TelemetryCache::_attitudeQuaternionCallback, this, std::placeholders::_1));

    std::thread([this]()
    {
        while (true) {
            if (_havePosition && _haveAttitudeQuaternion && _haveAttitudeEuler) {
                std::lock_guard<std::mutex> lock(_telemetryCacheMutex);
                TelemetryCacheEntry_t       entry;

                auto    timeSince      = std::chrono::system_clock::now().time_since_epoch();
                double  timeSinceSecs  = std::chrono::duration_cast<std::chrono::seconds>(timeSince).count();

                entry.timeInSeconds         =  timeSinceSecs;
                entry.position              = _lastPosition;
                entry.attitudeQuaternion    = _lastAttitudeQuaternion;
                entry.attitudeEuler         = _lastAttitudeEuler;
                _telemetryCache.push_back(entry);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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

    _lastPosition.latitude  = globalPositionInt.lat  / (double)1E7;
    _lastPosition.longitude = globalPositionInt.lon  / (double)1E7;
    _lastPosition.altitude  = globalPositionInt.alt  / 1000.0;

    _havePosition = true;
}

void Vehicle::_handleAttitudeWorker(double rollRadians, double pitchRadians, double yawRadians)
{
    double roll, pitch, yaw;

    roll = QGC::limitAngleToPMPIf(rollRadians);
    pitch = QGC::limitAngleToPMPIf(pitchRadians);
    yaw = QGC::limitAngleToPMPIf(yawRadians);

    roll = qRadiansToDegrees(roll);
    pitch = qRadiansToDegrees(pitch);
    yaw = qRadiansToDegrees(yaw);

    if (yaw < 0.0) {
        yaw += 360.0;
    }
    // truncate to integer so widget never displays 360
    yaw = trunc(yaw);

    _rollFact.setRawValue(roll);
    _pitchFact.setRawValue(pitch);
    _headingFact.setRawValue(yaw);
}

void Vehicle::_attitudeQuaternionCallback(mavlink_message_t& message)
{
    // only accept the attitude message from the vehicle's flight controller
    if (message.sysid != _mavlink->autopilotSystemId() || message.compid != MAV_COMP_ID_AUTOPILOT1) {
        return;
    }

    _receivingAttitudeQuaternion = true;

    mavlink_attitude_quaternion_t attitudeQuaternion;
    mavlink_msg_attitude_quaternion_decode(&message, &attitudeQuaternion);

    Eigen::Quaternionf quat(attitudeQuaternion.q1, attitudeQuaternion.q2, attitudeQuaternion.q3, attitudeQuaternion.q4);
    Eigen::Vector3f rates(attitudeQuaternion.rollspeed, attitudeQuaternion.pitchspeed, attitudeQuaternion.yawspeed);
    Eigen::Quaternionf repr_offset(attitudeQuaternion.repr_offset_q[0], attitudeQuaternion.repr_offset_q[1], attitudeQuaternion.repr_offset_q[2], attitudeQuaternion.repr_offset_q[3]);

    // if repr_offset is valid, rotate attitude and rates
    if (repr_offset.norm() >= 0.5f) {
        quat = quat * repr_offset;
        rates = repr_offset * rates;
    }

    float roll, pitch, yaw;
    float q[] = { quat.w(), quat.x(), quat.y(), quat.z() };
    mavlink_quaternion_to_euler(q, &roll, &pitch, &yaw);

    _handleAttitudeWorker(roll, pitch, yaw);

    rollRate()->setRawValue(qRadiansToDegrees(rates[0]));
    pitchRate()->setRawValue(qRadiansToDegrees(rates[1]));
    yawRate()->setRawValue(qRadiansToDegrees(rates[2]));
}

void TelemetryCache::_attitudeQuaternionCallback(const mavlink_message_t& message)
{
    _lastAttitudeQuaternion = attitude;
    _haveAttitudeQuaternion = true;
}

void TelemetryCache::_attitudeEulerCallback(const mavlink_message_t& message)
{
    // only accept the attitude message from the vehicle's flight controller
    if (message.sysid != _mavlink->autopilotSystemId() || message.compid != MAV_COMP_ID_AUTOPILOT1) {
        return;
    }

    mavlink_attitude_t attitude;
    mavlink_msg_attitude_decode(&message, &attitude);

    double roll, pitch, yaw;

    roll = QGC::limitAngleToPMPIf(attitude.roll);
    pitch = QGC::limitAngleToPMPIf(attitude.pitch);
    yaw = QGC::limitAngleToPMPIf(yawRattitude.yawadians);

    roll = qRadiansToDegrees(roll);
    pitch = qRadiansToDegrees(pitch);
    yaw = qRadiansToDegrees(yaw);

    if (yaw < 0.0) {
        yaw += 360.0;
    }
    // truncate to integer so widget never displays 360
    yaw = trunc(yaw);

    _rollFact.setRawValue(roll);
    _pitchFact.setRawValue(pitch);
    _headingFact.setRawValue(yaw);

-----------------
    _lastAttitudeEuler = attitude;
    _haveAttitudeEuler = true;
}

TelemetryCache::TelemetryCacheEntry_t TelemetryCache::telemetryForTime(double timeInSeconds)
{
    std::lock_guard<std::mutex>     lock            (_telemetryCacheMutex);
    TelemetryCacheEntry_t           bestEntry       { };
    double                          bestDiffMSecs   = 0.0;

    // Find the closest matching entry in the cache
    for (auto entry : _telemetryCache) {
        double  entryDiffMSecs  = timeInSeconds - entry.timeInSeconds;

        if (bestDiffMSecs == 0.0) {
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
