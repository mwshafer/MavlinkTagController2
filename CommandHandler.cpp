#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <future>
#include <memory>
#include <thread>

#include "CommandHandler.h"
#include "CommandDefs.h"
#include "startAirspyProcess.h"

using namespace mavsdk;

CommandHandler::CommandHandler(System& system, MavlinkPassthrough& mavlinkPassthrough)
    : _system               (system)
    , _mavlinkPassthrough   (mavlinkPassthrough)
{
    using namespace std::placeholders;

    _mavlinkPassthrough.intercept_incoming_messages_async(std::bind(&CommandHandler::_handleDebugFloatArray, this, _1));
}

void CommandHandler::_sendCommandAck(uint32_t commandId, uint32_t result)
{
    mavlink_message_t           message;
    mavlink_debug_float_array_t outgoingDebugFloatArray;

    memset(&outgoingDebugFloatArray, 0, sizeof(outgoingDebugFloatArray));

    outgoingDebugFloatArray.array_id              = COMMAND_ID_ACK;
    outgoingDebugFloatArray.data[ACK_IDX_COMMAND] = commandId;
    outgoingDebugFloatArray.data[ACK_IDX_RESULT]  = result;

    std::cout << "_sendCommandAck" << commandId << " " << result << std::endl;

    mavlink_msg_debug_float_array_encode(
        _mavlinkPassthrough.get_our_sysid(),
        _mavlinkPassthrough.get_our_compid(),
        &message,
        &outgoingDebugFloatArray);
    _mavlinkPassthrough.send_message(message);        
}

void CommandHandler::_handleTagCommand(const mavlink_debug_float_array_t& debugFloatArray)
{
    _tagInfo.tagId                  = static_cast<uint32_t>(debugFloatArray.data[TAG_IDX_ID]);
    _tagInfo.frequency              = static_cast<uint32_t>(debugFloatArray.data[TAG_IDX_FREQUENCY]);
    _tagInfo.pulseDuration          = static_cast<uint32_t>(debugFloatArray.data[TAG_IDX_DURATION_MSECS]);
    _tagInfo.intraPulse1            = static_cast<uint32_t>(debugFloatArray.data[TAG_IDX_INTRA_PULSE1_MSECS]);
    _tagInfo.intraPulse2            = static_cast<uint32_t>(debugFloatArray.data[TAG_IDX_INTRA_PULSE2_MSECS]);
    _tagInfo.intraPulseUncertainty  = static_cast<uint32_t>(debugFloatArray.data[TAG_IDX_INTRA_PULSE_UNCERTAINTY]);
    _tagInfo.intraPulseUncertainty  = static_cast<uint32_t>(debugFloatArray.data[TAG_IDX_INTRA_PULSE_JITTER]);
    _tagInfo.maxPulse               = debugFloatArray.data[TAG_IDX_MAX_PULSE];

    std::cout << "handleTagCommand: tagId:freq" << _tagInfo.tagId << " " << _tagInfo.frequency << std::endl;

    uint32_t commandResult = 1;

    if (_tagInfo.tagId == 0) {
        std::cout << "handleTagCommand: invalid tag id of 0" << std::endl;
        commandResult  = 0;
    }

    _sendCommandAck(COMMAND_ID_TAG, commandResult);
}

void CommandHandler::_handleStartDetection(const mavlink_debug_float_array_t& debugFloatArray)
{
    uint32_t requestedTagId = static_cast<uint32_t>(debugFloatArray.data[START_DETECTION_IDX_TAG_ID]);

    uint32_t commandResult = 1;

    if (requestedTagId == _tagInfo.tagId) {
        std::cout << "handleStartDetection: Detection started for freq " << _tagInfo.frequency << std::endl; 
    } else {
        std::cout << "handleStartDetection: requested start tag id != known tag id - requested:known " << 
            requestedTagId << " " <<
            _tagInfo.tagId << std::endl;
        commandResult  = 0;
    }

    _sendCommandAck(COMMAND_ID_START_DETECTION, commandResult);
}

void CommandHandler::_handleStopDetection(void)
{
    std::cout << "_handleStopDetection: Detection stopped" << std::endl; 
    _sendCommandAck(COMMAND_ID_STOP_DETECTION, 1);
}

bool CommandHandler::_handleDebugFloatArray(mavlink_message_t& message)
{
    std::thread* hfThread;
    std::thread* miniThread;

    if (message.msgid == MAVLINK_MSG_ID_DEBUG_FLOAT_ARRAY) {
        mavlink_debug_float_array_t debugFloatArray;

        mavlink_msg_debug_float_array_decode(&message, &debugFloatArray);
        switch (debugFloatArray.array_id) {
        case COMMAND_ID_TAG:
            _handleTagCommand(debugFloatArray);
            break;
        case COMMAND_ID_START_DETECTION:
            _handleStartDetection(debugFloatArray);
            break;
        case COMMAND_ID_STOP_DETECTION:
            _handleStopDetection();
            break;
        case COMMAND_ID_AIRSPY_HF:
            std::cout << "Start Airspy HF capture" << std::endl;
            hfThread = new std::thread(startAirspyHF);
            _sendCommandAck(COMMAND_ID_AIRSPY_HF, 1);
            break;
        case COMMAND_ID_AIRSPY_MINI:
            std::cout << "Start Airspy Mini" << std::endl;
            miniThread = new std::thread(startAirspyMini);
            _sendCommandAck(COMMAND_ID_AIRSPY_MINI, 1);
            break;
        }
    }

    return true;
}