#pragma once

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

using namespace mavsdk;

#include <string>
#include <memory>
#include <thread>
#include <atomic>

class UDPPulseReceiver
{
public:
	UDPPulseReceiver(std::string localIp, int localPort, MavlinkPassthrough& mavlinkPassthrough);
	~UDPPulseReceiver();

	void start 	(void);
	void stop 	(void);
	void receive(void);

private:
	bool _setupPort	(void);

    std::string 			_localIp;
    int 					_localPort;
    int 					_fdSocket	{-1};
    MavlinkPassthrough&     _mavlinkPassthrough;
};