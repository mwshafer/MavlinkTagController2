#pragma once

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

#include <thread>
#include <mutex>
#include <condition_variable>

class MavlinkOutgoingMessageQueue
{
public:
    MavlinkOutgoingMessageQueue(mavsdk::MavlinkPassthrough& mavlinkPassthrough);

    void                        addMessage          (const mavlink_message_t& message);
    mavsdk::MavlinkPassthrough& mavlinkPassthrough  (void) { return _mavlinkPassthrough; }

private:
	void _run(void);

private:
    mavsdk::MavlinkPassthrough&     _mavlinkPassthrough;
    std::vector<mavlink_message_t>  _messages;
    std::thread				        _thread;
    std::mutex                      _threadWaitMutex;
    std::condition_variable         _threadWaitCondition;
};