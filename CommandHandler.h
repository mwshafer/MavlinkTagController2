#pragma once

#include "TunnelProtocol.h"
#include "TagDatabase.h"
#include "MavlinkOutgoingMessageQueue.h"

#include <mavsdk/mavsdk.h>

#include <boost/process.hpp>

namespace bp = boost::process;

class MonitoredProcess;

class CommandHandler {
public:
    CommandHandler(mavsdk::System& system, MavlinkOutgoingMessageQueue& outgoingMessageQueue);

private:
    void _sendCommandAck        (uint32_t command, uint32_t result);
    bool _handleStartTags       (void);
    bool _handleEndTags         (void);
    bool _handleTag             (const mavlink_tunnel_t& tunnel);
    bool _handleStartDetection  (const mavlink_tunnel_t& tunnel);
    bool _handleStopDetection   (void);
    bool _handleAirspyMini      (void);
    void _handleTunnelMessage   (const mavlink_message_t& message);
    void _startDetector         (const TunnelProtocol::TagInfo_t& tagInfo, bool secondaryChannel);

    std::string _tunnelCommandIdToString    (uint32_t command);
    std::string _tunnelCommandResultToString(uint32_t result);

private:
    mavsdk::System&                 _system;
    MavlinkOutgoingMessageQueue&    _outgoingMessageQueue;
    TagDatabase                     _tagDatabase;
    bool                            _receivingTags      = false;
    bool                            _detectorsRunning   = false;
    char*                           _homePath           = NULL;
    std::vector<MonitoredProcess*>  _processes;
    uint                            _startCount         = 0;
    bp::pipe*                       _airspyPipe         = NULL;
};
