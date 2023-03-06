#pragma once

#include <thread>
#include <string>
#include <memory>
#include <thread>
#include <atomic>

#include "MavlinkOutgoingMessageQueue.h"

class UDPPulseReceiver
{
public:
	UDPPulseReceiver(std::string localIp, int localPort, MavlinkOutgoingMessageQueue& outgoingMessageQueue);
	~UDPPulseReceiver();

	void start	(void);
	void run 	(void);
	void stop 	(void);

private:
	bool _setupPort (void);
	void _receive 	(void);

	std::thread*					_thread 	{ nullptr };
    std::string 					_localIp;
    int 							_localPort;
    int 							_fdSocket	{-1};
    MavlinkOutgoingMessageQueue&    _outgoingMessageQueue;
};