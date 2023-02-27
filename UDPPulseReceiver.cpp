#include "UDPPulseReceiver.h"
#include "TunnelProtocol.h"
#include "sendTunnelMessage.h"
#include "formatString.h"

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


UDPPulseReceiver::UDPPulseReceiver(std::string localIp, int localPort, MavlinkPassthrough& mavlinkPassthrough)
    : _localIp	         (std::move(localIp))
    , _localPort         (localPort)
    , _mavlinkPassthrough(mavlinkPassthrough)
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
        std::cout << "socket error" << strerror(errno) << "\n";
        return false;
    }

    struct sockaddr_in addr {};
    addr.sin_family         = AF_INET;
    addr.sin_addr.s_addr    = inet_addr(_localIp.c_str());
    addr.sin_port           = htons(_localPort);

    if (bind(_fdSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cout << "bind error: " << strerror(errno) << std::endl;
        return false;
    }

    std::cout << "UDPPulseReceiver::_setupPort " << _localIp << " port: " << _localPort << std::endl;

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
            double group_ind;
            double group_snr;
            double detection_status;
            double confirmed_status;
        } UDPPulseInfo_T;

        UDPPulseInfo_T buffer[sizeof(UDPPulseInfo_T) * 10];

        auto cBytesReceived = recvfrom(_fdSocket, buffer, sizeof(buffer), 0, NULL, NULL);

        if (cBytesReceived < 0) {
            // This happens on destruction when close(_fdSocket) is called,
            // therefore be quiet.
            std::cout << "recvfrom error: " << strerror(errno) << std::endl;
            return;
        }

        int pulseCount = cBytesReceived / sizeof(UDPPulseInfo_T);
        int pulseIndex = 0;

        std::cout << pulseCount << std::endl;

        while (pulseCount--) {
            UDPPulseInfo_T udpPulseInfo = buffer[pulseIndex++];

            std::string pulseStatus = formatString("Id: %u Time: %.1f SNR: %.2f Conf: %u",
                                            (uint)udpPulseInfo.tag_id,
                                            udpPulseInfo.start_time_seconds,
                                            udpPulseInfo.snr,
                                            (uint)udpPulseInfo.confirmed_status);
            std::cout << pulseStatus << std::endl;

            PulseInfo_t pulseInfo;

            memset(&pulseInfo, 0, sizeof(pulseInfo));

            pulseInfo.header.command                = COMMAND_ID_PULSE;
            pulseInfo.tag_id                        = (uint32_t)udpPulseInfo.tag_id;
            pulseInfo.frequency_hz                  = (uint32_t)udpPulseInfo.frequency_hz;
            pulseInfo.start_time_seconds            = udpPulseInfo.start_time_seconds;
            pulseInfo.predict_next_start_seconds    = udpPulseInfo.predict_next_start_seconds;
            pulseInfo.snr                           = udpPulseInfo.snr;
            pulseInfo.stft_score                    = udpPulseInfo.stft_score;
            pulseInfo.group_ind                     = (uint16_t)udpPulseInfo.group_ind;
            pulseInfo.group_snr                     = udpPulseInfo.group_snr;
            pulseInfo.detection_status              = (uint8_t)udpPulseInfo.detection_status;
            pulseInfo.confirmed_status              = (uint8_t)udpPulseInfo.confirmed_status;

            sendTunnelMessage(_mavlinkPassthrough, &pulseInfo, sizeof(pulseInfo));

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
    }
}
