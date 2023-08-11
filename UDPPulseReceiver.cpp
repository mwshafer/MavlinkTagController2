#include "UDPPulseReceiver.h"
#include "TunnelProtocol.h"
#include "formatString.h"
#include "log.h"
#include "MavlinkSystem.h"
#include "TelemetryCache.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h> // for close()

#include <algorithm>
#include <utility>
#include <iostream>
#include <string.h>
#include <cstddef>
#include <chrono>

using namespace TunnelProtocol;


UDPPulseReceiver::UDPPulseReceiver(std::string localIp, int localPort, MavlinkSystem* mavlink, TelemetryCache* telemetryCache)
    : _localIp	            (std::move(localIp))
    , _localPort            (localPort)
    , _mavlink              (mavlink)
    , _telemetryCache       (telemetryCache)
{

}

UDPPulseReceiver::~UDPPulseReceiver()
{
    // If no one explicitly called stop before, we should at least do it.
    stop();
}


void UDPPulseReceiver::start()
{
    if (!_setupPort()) {
        return;
    }

    _thread = new std::thread(&UDPPulseReceiver::_receive, this);
}

bool UDPPulseReceiver::_setupPort(void)
{
    _fdSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (_fdSocket < 0) {
        logError() << "socket error" << strerror(errno);
        return false;
    }

    struct sockaddr_in addr {};
    addr.sin_family         = AF_INET;
    addr.sin_addr.s_addr    = inet_addr(_localIp.c_str());
    addr.sin_port           = htons(_localPort);

    if (bind(_fdSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        logError() << "bind error:" << strerror(errno);
        return false;
    }

    logDebug() << "UDPPulseReceiver::_setupPort" << _localIp << "port:" << _localPort;

    return true;
}

void UDPPulseReceiver::stop()
{
    // This should interrupt a recv/recvfrom call.
    shutdown(_fdSocket, SHUT_RDWR);

    // But on Mac, closing is also needed to stop blocking recv/recvfrom.
    close(_fdSocket);
}

void UDPPulseReceiver::_receive()
{
    while (true) {
        // Enough for MTU 1500 bytes.
        typedef struct {
            double tag_id;
            double frequency_hz;
            double start_time_seconds;
            double predict_next_start_seconds;
            double snr;
            double stft_score;
            double group_seq_counter;
            double group_ind;
            double group_snr;
            double detection_status;
            double confirmed_status;
            double noise_psd;
        } UDPPulseInfo_T;

        UDPPulseInfo_T buffer[sizeof(UDPPulseInfo_T) * 10];

        auto cBytesReceived = recvfrom(_fdSocket, buffer, sizeof(buffer), 0, NULL, NULL);

        if (cBytesReceived < 0) {
            // This happens on destruction when close(_fdSocket) is called,
            // therefore be quiet.
            logDebug() << "recvfrom error:" << strerror(errno);
            return;
        }

        int pulseCount = cBytesReceived / sizeof(UDPPulseInfo_T);
        int pulseIndex = 0;

        while (pulseCount--) {
            UDPPulseInfo_T udpPulseInfo = buffer[pulseIndex++];

            PulseInfo_t pulseInfo;

            memset(&pulseInfo, 0, sizeof(pulseInfo));

            auto telemetry = _telemetryCache->telemetryForTime(udpPulseInfo.start_time_seconds);

            pulseInfo.header.command                = COMMAND_ID_PULSE;
            pulseInfo.tag_id                        = (uint32_t)udpPulseInfo.tag_id;
            pulseInfo.frequency_hz                  = (uint32_t)udpPulseInfo.frequency_hz;
            pulseInfo.start_time_seconds            = udpPulseInfo.start_time_seconds;
            pulseInfo.predict_next_start_seconds    = udpPulseInfo.predict_next_start_seconds;
            pulseInfo.snr                           = udpPulseInfo.snr;
            pulseInfo.stft_score                    = udpPulseInfo.stft_score;
            pulseInfo.group_seq_counter             = (uint16_t)udpPulseInfo.group_seq_counter;
            pulseInfo.group_ind                     = (uint16_t)udpPulseInfo.group_ind;
            pulseInfo.group_snr                     = udpPulseInfo.group_snr;
            pulseInfo.detection_status              = (uint8_t)udpPulseInfo.detection_status;
            pulseInfo.confirmed_status              = (uint8_t)udpPulseInfo.confirmed_status;
            pulseInfo.position_x                    = telemetry.position.latitude;
            pulseInfo.position_y                    = telemetry.position.longitude;
            pulseInfo.position_z                    = telemetry.position.relativeAltitude;
            pulseInfo.orientation_x                 = telemetry.attitudeQuaternion.x;
            pulseInfo.orientation_y                 = telemetry.attitudeQuaternion.y;
            pulseInfo.orientation_z                 = telemetry.attitudeQuaternion.z;
            pulseInfo.orientation_w                 = telemetry.attitudeQuaternion.w;
            pulseInfo.noise_psd                     = udpPulseInfo.noise_psd;

            if (pulseInfo.frequency_hz == 0) {
                logInfo() << "HEARTBEAT from Detector" << pulseInfo.tag_id;
            } else {
                std::string pulseStatus = formatString("Conf: %u Id: %2u stft_score: %5.1f snr: %5.1f noise_psd: %5.1g seq_counter: %8u freq: %9u yaw/alt: %4.0f/%3.0f",
                                                pulseInfo.confirmed_status,
                                                pulseInfo.tag_id,
                                                pulseInfo.stft_score,
                                                pulseInfo.snr,
                                                pulseInfo.noise_psd,
                                                pulseInfo.group_seq_counter,
                                                pulseInfo.frequency_hz,
                                                telemetry.attitudeEuler.yawDegrees,
                                                telemetry.position.relativeAltitude);
                if (udpPulseInfo.confirmed_status) {
                    logInfo() << pulseStatus;
                } else {
                    logDebug() << pulseStatus;
                }
            }

            _mavlink->sendTunnelMessage(&pulseInfo, sizeof(pulseInfo));
        }
    }
}
