#include "UDPPulseReceiver.h"
#include "CommandDefs.h"

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
        std::cout << "bind error: " << strerror(errno) << "\n";
        return false;
    }

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
    } PulseInfo_T;

    PulseInfo_T buffer[sizeof(PulseInfo_T) * 10];

    auto cBytesReceived = recvfrom(_fdSocket, buffer, sizeof(buffer), 0, NULL, NULL);

    if (cBytesReceived < 0) {
        // This happens on destruction when close(_fdSocket) is called,
        // therefore be quiet.
        std::cout << "recvfrom error: " << strerror(errno);
        return;
    }

    int pulseCount = cBytesReceived / sizeof(PulseInfo_T);
    int pulseIndex = 0;

    while (pulseCount--) {
        PulseInfo_T pulseInfo = buffer[pulseIndex++];

    	std::cout << std::dec << std::fixed <<
            "Pulse Time: " << pulseInfo.timeSeconds <<
            " SNR: " << pulseInfo.snr << 
            " Conf: " << pulseInfo.confirmationStatus << 
            std::endl;

        mavlink_message_t           message;
        mavlink_debug_float_array_t debugFloatArray;

        memset(&debugFloatArray, 0, sizeof(debugFloatArray));

        debugFloatArray.time_usec                           = pulseInfo.timeSeconds * 1000000.0f;
        debugFloatArray.array_id                            = COMMAND_ID_PULSE;
        debugFloatArray.data[PULSE_IDX_SNR]                 = pulseInfo.snr;
        debugFloatArray.data[PULSE_IDX_CONFIRMED_STATUS]    = pulseInfo.confirmationStatus;

        *((double *)&debugFloatArray.data[PULSE_IDX_TIME_SECS]) = pulseInfo.timeSeconds;

        mavlink_msg_debug_float_array_encode(
            _mavlinkPassthrough.get_our_sysid(),
            _mavlinkPassthrough.get_our_compid(),
            &message,
            &debugFloatArray);
        _mavlinkPassthrough.send_message(message);        
    }
}
