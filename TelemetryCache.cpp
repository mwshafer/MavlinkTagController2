#include "TelemetryCache.h"

#include <functional>
#include <thread>
#include <chrono>

using namespace mavsdk;

TelemetryCache::TelemetryCache(System& system)
    : _system   (system)
    , _telemetry(system)
{
    _telemetry.subscribe_position           (std::bind(&TelemetryCache::_positionCallback, this, std::placeholders::_1));
    _telemetry.subscribe_attitude_quaternion(std::bind(&TelemetryCache::_attitudeQuaternionCallback, this, std::placeholders::_1));
    _telemetry.subscribe_attitude_euler     (std::bind(&TelemetryCache::_attitudeEulerCallback, this, std::placeholders::_1));

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

void TelemetryCache::_positionCallback(mavsdk::Telemetry::Position position)
{
    _lastPosition = position;
    _havePosition = true;
}

void TelemetryCache::_attitudeQuaternionCallback(mavsdk::Telemetry::Quaternion attitude)
{
    _lastAttitudeQuaternion = attitude;
    _haveAttitudeQuaternion = true;
}

void TelemetryCache::_attitudeEulerCallback(mavsdk::Telemetry::EulerAngle attitude)
{
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
