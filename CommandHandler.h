#pragma once

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

#include "TunnelProtocol.h"
#include "TagDatabase.h"

using namespace mavsdk;

class CommandHandler {
public:
    CommandHandler(System& system, MavlinkPassthrough& mavlinkPassthrough);

private:
    void _sendCommandAck        (uint32_t command, uint32_t result);
    bool _handleStartTags       (void);
    bool _handleEndTags         (void);
    bool _handleTag             (const mavlink_tunnel_t& tunnel);
    bool _handleStartDetection  (void);
    bool _handleStopDetection   (void);
    bool _handleAirspyMini      (void);
    void _handleTunnelMessage   (const mavlink_message_t& message);
    bool _writeDetectorConfig   (int tagIndex);

    std::string _detectorConfigFileName     (int tagIndex);
    std::string _detectorLogFileName        (int tagIndex);
    std::string _tunnelCommandIdToString    (uint32_t command);
    std::string _tunnelCommandResultToString(uint32_t result);

private:
    System&                     _system;
    MavlinkPassthrough&         _mavlinkPassthrough;
    TagDatabase                 _tagDatabase;
    bool                        _receivingTags      = false;
    bool                        _detectorsRunning   = false;
    char*                       _homePath           = NULL;
};
