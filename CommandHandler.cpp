#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <future>
#include <memory>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "CommandHandler.h"
#include "TunnelProtocol.h"
#include "sendTunnelMessage.h"
#include "sendStatusText.h"
#include "MonitoredProcess.h"
#include "formatString.h"
#include "log.h"
#include "channelizerTuner.h"

using namespace mavsdk;
using namespace TunnelProtocol;

CommandHandler::CommandHandler(System& system, MavlinkOutgoingMessageQueue& outgoingMessageQueue)
    : _system               (system)
    , _outgoingMessageQueue (outgoingMessageQueue)
    , _homePath             (getenv("HOME"))
{
    using namespace std::placeholders;

    outgoingMessageQueue.mavlinkPassthrough().subscribe_message_async(MAVLINK_MSG_ID_TUNNEL, std::bind(&CommandHandler::_handleTunnelMessage, this, _1));
}

void CommandHandler::_sendCommandAck(uint32_t command, uint32_t result)
{
    AckInfo_t           ackInfo;

    logDebug() << "_sendCommandAck command:result" << _tunnelCommandIdToString(command) << _tunnelCommandResultToString(result);

    ackInfo.header.command  = COMMAND_ID_ACK;
    ackInfo.command         = command;
    ackInfo.result          = result;

    sendTunnelMessage(_outgoingMessageQueue, &ackInfo, sizeof(ackInfo));
}

bool CommandHandler::_handleStartTags(void)
{
    logDebug() << "_handleStartTags _receivingTags:_detectorsRunnings" << _receivingTags << _detectorsRunning;

    if (_detectorsRunning) {
        return false;
    }

    if (_receivingTags) {
        sendStatusText(_outgoingMessageQueue, "Cancelling previous Start Tags sequence", MAV_SEVERITY_ALERT);
    }

    _tagDatabase.clear();
    _receivingTags = true;

    return true;
}

bool CommandHandler::_handleTag(const mavlink_tunnel_t& tunnel)
{
    TagInfo_t tagInfo;

    if (tunnel.payload_length != sizeof(tagInfo)) {
        logError() << "CommandHandler::_handleTagCommand ERROR - Payload length incorrect expected:actual" << sizeof(tagInfo) << tunnel.payload_length;
        return false;
    }

    memcpy(&tagInfo, tunnel.payload, sizeof(tagInfo));

    if (tagInfo.id < 2) {
        logError() << "CommandHandler::_handleTagCommand: invalid tag id of 0/1";
        return false;
    }

    logDebug() << "CommandHandler::handleTagCommand: id:freq:intra_pulse1_msecs " 
                << tagInfo.id
                << tagInfo.frequency_hz
                << tagInfo.intra_pulse1_msecs;

    _tagDatabase.push_back(tagInfo);

    return true;
}

bool CommandHandler::_handleEndTags(void)
{
    logDebug() << "_handleEndTags _receivingTags" << _receivingTags;

    if (!_receivingTags) {
        return false;
    }

    _receivingTags = false;

    std::thread([this]() {
        if (!_tagDatabase.writeDetectorConfigs()) {
            logError() << "CommandHandler::_handleEndTags: writeDetectorConfigs failed";
            sendStatusText(_outgoingMessageQueue, "Write Detector Configs failed", MAV_SEVERITY_ALERT);
        }
    }).detach();

    return true;
}

void CommandHandler::_startDetector(const TunnelProtocol::TagInfo_t& tagInfo, bool secondaryChannel)
{
    std::string commandStr  = formatString("%s/repos/uavrt_detection/uavrt_detection %s",
                                _homePath,
                                _tagDatabase.detectorConfigFileName(tagInfo, secondaryChannel).c_str());
    std::string logPath     = formatString("%s/detector.%d.%u.log", _homePath, tagInfo.id + (secondaryChannel ? 1 : 0), _startCount);

    MonitoredProcess* detectorProc = new MonitoredProcess(
                                                _outgoingMessageQueue, 
                                                "uavrt_detection", 
                                                commandStr.c_str(), 
                                                logPath.c_str(), 
                                                MonitoredProcess::NoPipe,
                                                NULL);
    detectorProc->start();  
    _processes.push_back(detectorProc);
}

bool CommandHandler::_handleStartDetection(const mavlink_tunnel_t& tunnel)
{
    StartDetectionInfo_t startDetection;

   if (tunnel.payload_length != sizeof(startDetection)) {
        logError() << "COMMAND_ID_START_DETECTION - ERROR: Payload length incorrect expected:actual" << sizeof(startDetection) << tunnel.payload_length;
        return false;
    }

    if (_receivingTags) {
        sendStatusText(_outgoingMessageQueue, "Start detection failed. In the middle fo tag receive sequence.", MAV_SEVERITY_ALERT);
        return false;
    }
    if (_detectorsRunning) {
        sendStatusText(_outgoingMessageQueue, "Start detection failed. Detectors are already running.", MAV_SEVERITY_ALERT);
        return false;
    }
    if (_tagDatabase.size() == 0) {
        sendStatusText(_outgoingMessageQueue, "Start detection failed. No tags sent to vehicle.", MAV_SEVERITY_ALERT);
        return false;
    }

    std::thread([this, &startDetection, tunnel]() {
 
        memcpy(&startDetection, tunnel.payload, sizeof(startDetection));

        logInfo() << "COMMAND_ID_START_DETECTION:";
        logInfo() << "\t_startCount:" << ++_startCount; 
        logInfo() << "\tradio_center_frequency_hz:" << startDetection.radio_center_frequency_hz; 

        _airspyPipe = new bp::pipe();

        std::string commandStr  = formatString("airspy_rx -f %f -a 3000000 -h 21 -t 0 -r /dev/stdout",
                                    (double)startDetection.radio_center_frequency_hz / 1000000.0);
        std::string logPath     = formatString("%s/airspy_rx.%u.log", _homePath, _startCount);
        MonitoredProcess* airspyProc = new MonitoredProcess(
                                                _outgoingMessageQueue, 
                                                "airspy_rx", 
                                                commandStr.c_str(), 
                                                logPath.c_str(), 
                                                MonitoredProcess::OutputPipe,
                                                _airspyPipe);
        airspyProc->start();
        _processes.push_back(airspyProc);

        logPath = formatString("%s/csdr-uavrt.%u.log", _homePath, _startCount);
        MonitoredProcess* csdrProc = new MonitoredProcess(
                                                _outgoingMessageQueue, 
                                                "csdr-uavrt", 
                                                "csdr-uavrt fir_decimate_cc 8 0.05 HAMMING", 
                                                logPath.c_str(), 
                                                MonitoredProcess::InputPipe,
                                                _airspyPipe);
        csdrProc->start();
        _processes.push_back(csdrProc);

        commandStr  = formatString("%s/repos/airspy_channelize/airspy_channelize %s", _homePath, _tagDatabase.channelizerCommandLine().c_str());
        logPath = formatString("%s/airspy_channelize.%u.log", _homePath, _startCount);
        MonitoredProcess* channelizeProc = new MonitoredProcess(
                                                    _outgoingMessageQueue, 
                                                    "airspy_channelize", 
                                                    commandStr.c_str(), 
                                                    logPath.c_str(), 
                                                    MonitoredProcess::NoPipe,
                                                    NULL);
        channelizeProc->start();
        _processes.push_back(channelizeProc);

        for (const TunnelProtocol::TagInfo_t& tagInfo: _tagDatabase) {
            _startDetector(tagInfo, false /* secondaryChannel */);
            if (tagInfo.intra_pulse2_msecs != 0) {
                _startDetector(tagInfo, true /* secondaryChannel */);
            }
        }

        _detectorsRunning = true;
    }).detach();

    return true;
}

bool CommandHandler::_handleStopDetection(void)
{
    logDebug() << "_handleStopDetection _detectorsRunnings" << _detectorsRunning;

    if (!_detectorsRunning) {
        sendStatusText(_outgoingMessageQueue, "Detectors not running", MAV_SEVERITY_ALERT);
        return false;
    }

    std::thread([this]() {
        for (MonitoredProcess* process: _processes) {
            process->stop();
        }
        _processes.clear();
        _detectorsRunning = false;
        delete _airspyPipe;
        _airspyPipe = NULL;
        sendStatusText(_outgoingMessageQueue, "All processes stopped", MAV_SEVERITY_ALERT);
    }).detach();

    return true;
}

bool CommandHandler::_handleAirspyMini(void)
{
    std::shared_ptr<bp::pipe>   dummyPipe;
    std::string                 commandStr = formatString("airspy_rx -r %s/airspy_mini.dat -f 146 -a 3000000 -h 21 -t 0 -n 90000000", _homePath);
    std::string                 logPath    = formatString("%s/airspy-mini-capture.log", _homePath);

    MonitoredProcess* airspyProcess = new MonitoredProcess(
                                                _outgoingMessageQueue, 
                                                "mini-capture", 
                                                commandStr.c_str(), 
                                                logPath.c_str(), 
                                                MonitoredProcess::NoPipe,
                                                NULL);
    airspyProcess->start();

    return true;
}

void CommandHandler::_handleTunnelMessage(const mavlink_message_t& message)
{
    mavlink_tunnel_t tunnel;

    mavlink_msg_tunnel_decode(&message, &tunnel);

    HeaderInfo_t headerInfo;

    if (tunnel.payload_length < sizeof(headerInfo)) {
        logError() << "CommandHandler::_handleTunnelMessage payload too small";
        return;
    }

    memcpy(&headerInfo, tunnel.payload, sizeof(headerInfo));

    bool success = false;

    switch (headerInfo.command) {
    case COMMAND_ID_START_TAGS:
        success = _handleStartTags();
        break;
    case COMMAND_ID_END_TAGS:
        success = _handleEndTags();
        break;
    case COMMAND_ID_TAG:
        success = _handleTag(tunnel);
        break;
    case COMMAND_ID_START_DETECTION:
        success = _handleStartDetection(tunnel);
        break;
    case COMMAND_ID_STOP_DETECTION:
        success = _handleStopDetection();
        break;
    case COMMAND_ID_AIRSPY_MINI:
        success = _handleAirspyMini();
        break;
    }

    _sendCommandAck(headerInfo.command, success ? COMMAND_RESULT_SUCCESS : COMMAND_RESULT_FAILURE);
}

std::string CommandHandler::_tunnelCommandIdToString(uint32_t command)
{
    std::string commandStr;

    switch (command) {
    case COMMAND_ID_ACK:
        commandStr = "ACK";
        break;
    case COMMAND_ID_START_TAGS:
        commandStr = "START_TAGS";
        break;
    case COMMAND_ID_END_TAGS:
        commandStr = "END_TAGS";
        break;
    case COMMAND_ID_TAG:
        commandStr = "TAG";
        break;
    case COMMAND_ID_START_DETECTION:
        commandStr = "START_DETECTION";
        break;
    case COMMAND_ID_STOP_DETECTION:
        commandStr = "STOP_DETECTION";
        break;
    case COMMAND_ID_PULSE:
        commandStr = "PULSE";
        break;
    case COMMAND_ID_AIRSPY_HF:
        commandStr = "AIRSPY_HF";
        break;
    case COMMAND_ID_AIRSPY_MINI:
        commandStr = "AIRSPY_MINI";
        break;
    }

    return commandStr;
}

std::string CommandHandler::_tunnelCommandResultToString(uint32_t result)
{
    std::string resultStr;

    switch (result) {
    case COMMAND_RESULT_SUCCESS:
        resultStr = "SUCCESS";
        break;
    case COMMAND_RESULT_FAILURE:
        resultStr = "FAILURE";
        break;
    }

    return resultStr;
}
