#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <future>
#include <memory>
#include <thread>

#include "CommandHandler.h"
#include "TunnelProtocol.h"
#include "startAirspyProcess.h"
#include "sendTunnelMessage.h"

using namespace mavsdk;
using namespace TunnelProtocol;

CommandHandler::CommandHandler(System& system, MavlinkPassthrough& mavlinkPassthrough)
    : _system               (system)
    , _mavlinkPassthrough   (mavlinkPassthrough)
{
    using namespace std::placeholders;

    _mavlinkPassthrough.subscribe_message_async(MAVLINK_MSG_ID_TUNNEL, std::bind(&CommandHandler::_handleTunnelMessage, this, _1));
}

void CommandHandler::_sendCommandAck(uint32_t command, uint32_t result)
{
    AckInfo_t           ackInfo;

    std::cerr << "_sendCommandAck command:result " << command << " " << result << std::endl;

    ackInfo.header.command  = COMMAND_ID_ACK;
    ackInfo.command         = command;
    ackInfo.result          = result;

    sendTunnelMessage(_mavlinkPassthrough, &ackInfo, sizeof(ackInfo));
}


void CommandHandler::_handleTagCommand(const mavlink_tunnel_t& tunnel)
{
    if (tunnel.payload_length != sizeof(_tagInfo)) {
        std::cout << "CommandHandler::_handleTagCommand ERROR - Payload length incorrect expected:actual " << sizeof(_tagInfo) << " " << tunnel.payload_length;
        return;
    }

    memcpy(&_tagInfo, tunnel.payload, sizeof(_tagInfo));

    std::cout << "handleTagCommand: id:freq" << _tagInfo.id << " " << _tagInfo.frequencyHz << std::endl;

    uint32_t commandResult = COMMAND_RESULT_SUCCESS;

    if (_tagInfo.id == 0) {
        std::cout << "handleTagCommand: invalid tag id of 0" << std::endl;
        commandResult  = COMMAND_RESULT_FAILURE;
    }

    _sendCommandAck(COMMAND_ID_TAG, commandResult);
}

void CommandHandler::_handleStartDetection(void)
{
    std::cout << "handleStartDetection: NYI " << std::endl; 
    _sendCommandAck(COMMAND_ID_START_DETECTION, COMMAND_RESULT_SUCCESS);
}

void CommandHandler::_handleStopDetection(void)
{
    std::cout << "handleStopDetection: NYI " << std::endl; 
    _sendCommandAck(COMMAND_ID_STOP_DETECTION, COMMAND_RESULT_SUCCESS);
}

void CommandHandler::_handleTunnelMessage(const mavlink_message_t& message)
{
    mavlink_tunnel_t tunnel;

    mavlink_msg_tunnel_decode(&message, &tunnel);

    HeaderInfo_t headerInfo;

    if (tunnel.payload_length < sizeof(headerInfo)) {
        std::cout << "CommandHandler::_handleTunnelMessage payload too small" << std::endl;
        return;
    }

    memcpy(&headerInfo, tunnel.payload, sizeof(headerInfo));

    switch (headerInfo.command) {
    case COMMAND_ID_TAG:
        _handleTagCommand(tunnel);
        break;
    case COMMAND_ID_START_DETECTION:
        _handleStartDetection();
        break;
    case COMMAND_ID_STOP_DETECTION:
        _handleStopDetection();
        break;
    case COMMAND_ID_AIRSPY_HF:
        std::cout << "Start Airspy HF capture" << std::endl;
        new std::thread(startAirspyHF);
        _sendCommandAck(COMMAND_ID_AIRSPY_HF, COMMAND_RESULT_SUCCESS);
        break;
    case COMMAND_ID_AIRSPY_MINI:
        std::cout << "Start Airspy Mini" << std::endl;
        new std::thread(startAirspyMini);
        _sendCommandAck(COMMAND_ID_AIRSPY_MINI, COMMAND_RESULT_SUCCESS);
        break;
    }

}