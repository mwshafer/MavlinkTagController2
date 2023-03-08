#include "TagDatabase.h"
#include "log.h"
#include "formatString.h"

#include <algorithm>
#include <numeric>
#include <functional>
#include <errno.h>
#include <cstring>
#include <math.h>

std::string TagDatabase::detectorConfigFileName(const TunnelProtocol::TagInfo_t& tagInfo, bool secondaryChannel) const
{
    return formatString("%s/detector.%d.config", getenv("HOME"), tagInfo.id + (secondaryChannel ? 1 : 0));
}

bool TagDatabase::_writeDetectorConfig(const TunnelProtocol::TagInfo_t& tagInfo, bool secondaryChannel) const
{
    const char* homePath = getenv("HOME");

    std::string configPath  = detectorConfigFileName(tagInfo, secondaryChannel);

    FILE* fp = fopen(configPath.c_str(), "w");
    if (fp == NULL) {
        logError() << "_writeDetectorConfig fopen failed -" << strerror(errno);
        return false;
    }

    int     secondaryChannelIncrement   = secondaryChannel ? 1 : 0;
    auto    tagId                       = tagInfo.id + secondaryChannelIncrement;
    double  channelCenterFreqMHz        = double(tagInfo.channelizer_channel_center_frequency_hz) / 1000000.0;
    double  tagFreqMHz                  = double(tagInfo.frequency_hz) / 1000000.0;
    auto    tip_msecs                   = secondaryChannel ? tagInfo.intra_pulse2_msecs : tagInfo.intra_pulse1_msecs;
    auto    portData                    = 20000 + ((tagInfo.channelizer_channel_number - 1) * 2) + secondaryChannelIncrement;
    auto    tip                         = double(tip_msecs) / 1000.0;

    logInfo() << "DETECTOR CONFIG:" << configPath;
    logInfo() << "\ttagId:                      " << tagId;
    logInfo() << "\tchannelCenterFreqMHz:       " << tagInfo.channelizer_channel_center_frequency_hz;
    logInfo() << "\tchannelizer_channel_number: " << tagInfo.channelizer_channel_number;
    logInfo() << "\ttagFreqMHz:                 " << tagInfo.frequency_hz;
    logInfo() << "\tportData:                   " << portData;

    fprintf(fp, "##################################################\n");
    fprintf(fp, "ID:\t%d\n",                                tagId);
    fprintf(fp, "channelCenterFreqMHz:\t%f\n",              channelCenterFreqMHz);
    fprintf(fp, "ipData:\t127.0.0.1\n");
    fprintf(fp, "portData:\t%d\n",                          portData);
    fprintf(fp, "Fs:\t3750\n");
    fprintf(fp, "tagFreqMHz:\t%f\n",                        tagFreqMHz);
    fprintf(fp, "tp:\t0.015\n");
    fprintf(fp, "tip:\t%f\n",                               tip);
    fprintf(fp, "tipu:\t0.06000\n");
    fprintf(fp, "tipj:\t0.020000\n");
    fprintf(fp, "K:\t3\n");
    fprintf(fp, "opMode:\tfreqSearchHardLock\n");
    fprintf(fp, "excldFreqs:\t[Inf, -Inf]\n");
    fprintf(fp, "falseAlarmProb:\t0.05\n");
    fprintf(fp, "dataRecordPath:\t%s/data_record.%d.bin\n", homePath, tagId);
    fprintf(fp, "ipCntrl:\t127.0.0.1\n");
    fprintf(fp, "portCntrl:\t30000\n");
    fprintf(fp, "processedOuputPath:\t%s\n",                homePath);
    fprintf(fp, "ros2enable:\tfalse\n");
    fprintf(fp, "startInRunState:\ttrue\n");
    fprintf(fp, "timeStamp:\t1646403180.469\n");

    fclose(fp);

    return true;
}

bool TagDatabase::writeDetectorConfigs() const
{
    for (const auto& tagInfo : *this) {
        _writeDetectorConfig(tagInfo, false);
        if (tagInfo.intra_pulse2_msecs != 0) {
            _writeDetectorConfig(tagInfo, true);
        }
    }

    return true;
}

std::string TagDatabase::channelizerCommandLine() const
{
    std::string commandLine;

    for (const TunnelProtocol::TagInfo_t& tagInfo : *this) {
        commandLine += formatString("%d ", tagInfo.channelizer_channel_number * (tagInfo.intra_pulse2_msecs != 0 ? -1 : 1));
    }

    return commandLine;
}
