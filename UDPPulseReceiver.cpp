#include "UDPPulseReceiver.h"
#include "TunnelProtocol.h"
#include "sendTunnelMessage.h"

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

void UDPPulseReceiver::receive()
{
    // Enough for MTU 1500 bytes.
    typedef struct {
        double snr;
        double confirmationStatus;
        double timeSeconds;
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

    while (pulseCount--) {
        UDPPulseInfo_T udpPulseInfo = buffer[pulseIndex++];

    	std::cout << std::dec << std::fixed <<
            "Pulse Time: " << udpPulseInfo.timeSeconds <<
            " SNR: " << udpPulseInfo.snr << 
            " Conf: " << udpPulseInfo.confirmationStatus << 
            std::endl;

        PulseInfo_t pulseInfo;

        memset(&pulseInfo, 0, sizeof(pulseInfo));

        pulseInfo.header.command            = COMMAND_ID_PULSE;
        pulseInfo.start_time_seconds        = udpPulseInfo.timeSeconds;
        pulseInfo.snr = udpPulseInfo.snr    = udpPulseInfo.snr;
        pulseInfo.confirmed_status          = udpPulseInfo.confirmationStatus;

        sendTunnelMessage(_mavlinkPassthrough, &pulseInfo, sizeof(pulseInfo));
    }
}
