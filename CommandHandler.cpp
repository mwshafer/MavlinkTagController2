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

using namespace mavsdk;
using namespace TunnelProtocol;

CommandHandler::CommandHandler(System& system, MavlinkPassthrough& mavlinkPassthrough)
    : _system               (system)
    , _mavlinkPassthrough   (mavlinkPassthrough)
    , _homePath             (getenv("HOME"))
{
    using namespace std::placeholders;

    _mavlinkPassthrough.subscribe_message_async(MAVLINK_MSG_ID_TUNNEL, std::bind(&CommandHandler::_handleTunnelMessage, this, _1));
}

void CommandHandler::_sendCommandAck(uint32_t command, uint32_t result)
{
    AckInfo_t           ackInfo;

    logDebug() << "_sendCommandAck command:result" << _tunnelCommandIdToString(command) << _tunnelCommandResultToString(result);

    ackInfo.header.command  = COMMAND_ID_ACK;
    ackInfo.command         = command;
    ackInfo.result          = result;

    sendTunnelMessage(_mavlinkPassthrough, &ackInfo, sizeof(ackInfo));
}

bool CommandHandler::_handleStartTags(void)
{
    logDebug() << "_handleStartTags _receivingTags:_detectorsRunnings" << _receivingTags << _detectorsRunning;

    if (_detectorsRunning) {
        return false;
    }

    if (_receivingTags) {
        sendStatusText(_mavlinkPassthrough, "Cancelling previous Start Tags sequence", MAV_SEVERITY_ALERT);
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

    if (_tagDatabase.size() == 2) {
        logError() << "CommandHandler::_handleTagCommand: Only two tags supported";
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

    if (_receivingTags) {
        _receivingTags = false;
        if (!_writeDetectorConfig(0) || !_writeDetectorConfig(1)) {
            return false;
        }
        remove(_detectorLogFileName(0).c_str());
        remove(_detectorLogFileName(1).c_str());
        return true;
    } else {
        return false;
    }
}

bool CommandHandler::_handleStartDetection(void)
{
    logDebug() << "_handleStartDetection _receivingTags:_detectorsRunnings:_tagDatabase.size" 
        << _receivingTags
        << _detectorsRunning
        << _tagDatabase.size();

    if (_receivingTags || _detectorsRunning || _tagDatabase.size() == 0) {
        return false;
    }

    std::shared_ptr<bp::pipe> intermediatePipe;

    std::string commandStr  = formatString("airspy_rx -f %f -a 3000000 -h 21 -t 0 -r /dev/stdout",
                                (double)_tagDatabase[0].frequency_hz / 1000000.0);
    std::string logPath     = formatString("%s/airspy_rx.log",
                                _homePath);
    MonitoredProcess* airspyProc = new MonitoredProcess(
                                            _mavlinkPassthrough, 
                                            "airspy_rx", 
                                            commandStr.c_str(), 
                                            logPath.c_str(), 
                                            MonitoredProcess::OutputPipe,
                                            intermediatePipe);
    airspyProc->start();

    logPath = formatString("%s/csdr-uavrt.log", _homePath);
    MonitoredProcess* csdrProc = new MonitoredProcess(
                                            _mavlinkPassthrough, 
                                            "csdr-uavrt", 
                                            "csdr-uavrt fir_decimate_cc 8 0.05 HAMMING", 
                                            logPath.c_str(), 
                                            MonitoredProcess::InputPipe,
                                            intermediatePipe);
    csdrProc->start();

    std::shared_ptr<bp::pipe> dummyPipe;
    commandStr  = formatString("%s/repos/airspy_channelize/airspy_channelize -1", _homePath);
    logPath = formatString("%s/airspy_channelize.log", _homePath);
    MonitoredProcess* channelizeProc = new MonitoredProcess(
                                                _mavlinkPassthrough, 
                                                "airspy_channelize", 
                                                commandStr.c_str(), 
                                                logPath.c_str(), 
                                                MonitoredProcess::NoPipe,
                                                dummyPipe);
    channelizeProc->start();

    for (size_t i=0; i<_tagDatabase.size(); i++) {

        // FIXME: We leak MonitoredProcess objects here
        commandStr  = formatString("%s/repos/uavrt_detection/uavrt_detection %s",
                        _homePath,
                        _detectorConfigFileName(i).c_str());
        logPath     = _detectorLogFileName(i);

        MonitoredProcess* detectorProc1 = new MonitoredProcess(
                                                    _mavlinkPassthrough, 
                                                    "uavrt_detection", 
                                                    commandStr.c_str(), 
                                                    logPath.c_str(), 
                                                    MonitoredProcess::NoPipe,
                                                    dummyPipe);
        detectorProc1->start();
    }

    _detectorsRunning = true;

    return true;
}

bool CommandHandler::_handleStopDetection(void)
{
    logDebug() << "_handleStopDetection _detectorsRunnings" << _detectorsRunning;

    _sendCommandAck(COMMAND_ID_STOP_DETECTION, COMMAND_RESULT_SUCCESS);

    return true;
}

bool CommandHandler::_handleAirspyMini(void)
{
    std::shared_ptr<bp::pipe>   dummyPipe;
    std::string                 commandStr = formatString("airspy_rx -r %s/airspy_mini.dat -f 146 -a 3000000 -h 21 -t 0 -n 90000000", _homePath);
    std::string                 logPath    = formatString("%s/airspy-mini-capture.log", _homePath);

    MonitoredProcess* airspyProcess = new MonitoredProcess(
                                                _mavlinkPassthrough, 
                                                "mini-capture", 
                                                commandStr.c_str(), 
                                                logPath.c_str(), 
                                                MonitoredProcess::NoPipe,
                                                dummyPipe);
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
        success = _handleStartDetection();
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

bool CommandHandler::_writeDetectorConfig(int tagIndex)
{
    TagInfo_t&  tagInfo     = _tagDatabase[tagIndex];
    std::string configPath  = _detectorConfigFileName(tagIndex);

    logDebug() << "_writeDetectorConfig" << configPath;

    FILE* fp = fopen(configPath.c_str(), "w");
    if (fp == NULL) {
        logError() << "_writeDetectorConfig fopen failed -" << strerror(errno);
        return false;
    }

    double freqMHz = double(tagInfo.frequency_hz) / 1000000.0;

    fprintf(fp, "##################################################\n");
    fprintf(fp, "ID:\t%d\n", tagInfo.id);
    fprintf(fp, "channelCenterFreqMHz:\t%f\n", freqMHz);
    fprintf(fp, "timeStamp:\t1646403180.469\n");
    fprintf(fp, "ipData:\t127.0.0.1\n");
    fprintf(fp, "portData:\t%d\n", 20000 + tagIndex);
    fprintf(fp, "ipCntrl:\t127.0.0.1\n");
    fprintf(fp, "portCntrl:\t30000\n");
    fprintf(fp, "Fs:\t3750\n");
    fprintf(fp, "tagFreqMHz:\t%f\n", freqMHz);
    fprintf(fp, "tp:\t0.015\n");
    fprintf(fp, "tip:\t%f\n", double(tagInfo.intra_pulse1_msecs) / 1000.0);
    fprintf(fp, "tipu:\t0.06000\n");
    fprintf(fp, "tipj:\t0.020000\n");
    fprintf(fp, "K:\t3\n");
    fprintf(fp, "opMode:\tfreqSearchHardLock\n");
    fprintf(fp, "excldFreqs:\t[Inf, -Inf]\n");
    fprintf(fp, "falseAlarmProb:\t0.05\n");
    fprintf(fp, "dataRecordPath:\t%s/data_record.bin\n", _homePath);
    fprintf(fp, "processedOuputPath:\t%s\n", _homePath);
    fprintf(fp, "ros2enable:\tfalse\n");
    fprintf(fp, "startInRunState:\ttrue\n");

    fclose(fp);

    return true;
}

std::string CommandHandler::_detectorConfigFileName(int tagIndex)
{
    return formatString("%s/detector.%d.config", _homePath, _tagDatabase[tagIndex].id);
}

std::string CommandHandler::_detectorLogFileName(int tagIndex)
{
    return formatString("%s/detector.%d.log", _homePath, _tagDatabase[tagIndex].id);
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
