#include "TagDatabase.h"
#include "log.h"
#include "formatString.h"

#include <algorithm>
#include <numeric>
#include <functional>
#include <errno.h>
#include <cstring>

void TagDatabase::addTag(const TunnelProtocol::TagInfo_t& tagInfo)
{
    ExtTagInfo_t extTagInfo;
    extTagInfo.tagInfo = tagInfo;
    extTagInfo.oneBasedChannelBin = 0;
    push_back(extTagInfo);
}

bool TagDatabase::channelizerTuner(
        const uint32_t              sampleRateHz, 
        const uint32_t              nChannels, 
        uint32_t&                   bestCenterHz)
{
    // Build the list of requested frequencies
    std::vector<uint32_t> freqListHz;
    freqListHz.resize(size());
    std::transform(begin(), end(), freqListHz.begin(), [](const ExtTagInfo_t& tag) { return tag.tagInfo.frequency_hz; });

    uint32_t freqMaxHz          = *std::max_element(freqListHz.begin(), freqListHz.end());
    uint32_t freqMinHz          = *std::min_element(freqListHz.begin(), freqListHz.end());
    uint32_t fullBwHz           = sampleRateHz;
    uint32_t halfBwHz           = fullBwHz / 2;
    uint32_t channelBwHz        = sampleRateHz / nChannels;
    uint32_t halfChannelBwHz    = channelBwHz / 2;

    // First make sure all the requested frequencies can fit within the available bandwidth
    if (freqMaxHz - freqMinHz > sampleRateHz) {
        logError() << "Requested frequencies are too far apart to fit within the available bandwidth";
        return false;
    }

    // If there is only a single requested frequency, then we just position it within the center of a
    // channel which is nearest to center.
    if (freqListHz.size() == 1) {
        auto singleFreqHz   = freqListHz[0];
        auto channelBin     = nChannels / 2;

        if (nChannels % 2 == 0) {
            bestCenterHz = singleFreqHz + halfChannelBwHz;
        } else {
            bestCenterHz = singleFreqHz;
        }

        (*this)[0].oneBasedChannelBin  = channelBin + 1;

        return true;
    }


    // We determine best possible center frequencies by looping through possible choices and assigning
    // weights to each.

    // Generate the list of center channel choices to test against
    std::vector<uint32_t> testCentersHz;
    uint32_t stepSizeHz = 1000;
    uint32_t nextPossibleHz = freqMinHz + halfChannelBwHz;
    while (nextPossibleHz <= freqMaxHz - halfChannelBwHz) {
        testCentersHz.push_back(nextPossibleHz);
        nextPossibleHz += stepSizeHz;
    }
    logDebug() << "testCentersHz:" << testCentersHz;

    std::vector<bool>                   usableTestCenters;
    std::vector<uint32_t>               channelUsageCountsForTestCenter;
    std::vector<uint32_t>               distanceFromCenterAverages;
    std::vector<std::vector<uint32_t>>  channelBinsArray;
    
    std::vector<bool>                   isFreqInShoulder;
    std::vector<std::vector<uint32_t>>  channelUsageCountsArray;
    channelUsageCountsForTestCenter.resize(nChannels);

    for (auto testCenterHz : testCentersHz) {
        std::for_each(channelUsageCountsForTestCenter.begin(), channelUsageCountsForTestCenter.end(), [](uint32_t& n) { n = 0; });

        std::cout << "Channels edges: ";
        int nextChannelStartFreqHz = testCenterHz - halfBwHz;
        for (uint32_t i=0; i<nChannels; i++) {
            std::cout << nextChannelStartFreqHz << " ";
            nextChannelStartFreqHz += channelBwHz;
        }
        std::cout << std::endl;

        std::vector<uint32_t> distanceFromCenters;
        std::vector<uint32_t> channelBins;

        for (auto requestedFreqHz : freqListHz) {
            // Adjust the requested frequency to be zero-based withing the available total bandwidth
            int adjustedFreqHz = requestedFreqHz - (testCenterHz - halfBwHz);

            // Find the frequency position within the channel bandwidth
            auto hzPositionWithinChannel = adjustedFreqHz % channelBwHz;

            // Find the distance from the center of the channel
            auto distanceFromCenter = abs(hzPositionWithinChannel - halfChannelBwHz);

            distanceFromCenters.push_back(distanceFromCenter);

            // Keep track of the number of frequencies which are within each channel bandwidth
            auto channelBucket = adjustedFreqHz / channelBwHz;

            logDebug() << "requestedFreqHz:" << requestedFreqHz << 
                "testCenterHz:" << testCenterHz << 
                "adjustedFreqHz:" << adjustedFreqHz << 
                "halfBwHz:" << halfBwHz <<
                "channelBwHz:" << channelBwHz <<
                "hzPositionWithinChannel:" << hzPositionWithinChannel <<
                "distanceFromCenter:" << distanceFromCenter <<
                "channelBins:" << channelBins <<
                "channelBucket:" << channelBucket;

            channelBins.push_back(channelBucket + 1);

            channelUsageCountsForTestCenter[channelBucket]++;
        }

        // Find the average of the distances from the center of each channel
        auto averageDistanceFromCenter = std::accumulate(distanceFromCenters.begin(), distanceFromCenters.end(), 0) / distanceFromCenters.size();
        distanceFromCenterAverages.push_back(averageDistanceFromCenter);

        channelBinsArray.push_back(channelBins);
    }

    // Find the best center frequency

    uint32_t                smallestDistanceFromCenterAverage   = channelBwHz;
    uint32_t                bestTestCenterHz                    = 0;
    std::vector<uint32_t>   channelBins;
    for (size_t i = 0; i < testCentersHz.size(); i++) {
        auto testCenterHz = testCentersHz[i];

        // We can't use a center frequency if it has more than one frequency in the same channel
        if (std::any_of(channelUsageCountsForTestCenter.begin(), channelUsageCountsForTestCenter.end(), [](uint32_t n) { return n > 1; })) {
            continue;
        }

        // We can't use a center frequency if it has a requested frequency within the shoulder of the channel
        double  shoulderPercent = 0.925;
        auto    centerBwHz      = channelBwHz * shoulderPercent;
        if (std::any_of(distanceFromCenterAverages.begin(), distanceFromCenterAverages.end(), 
                std::bind([](uint32_t n, uint32_t centerBwHz) { return n > centerBwHz; }, std::placeholders::_1, centerBwHz))) {
            continue;
        }

        if (distanceFromCenterAverages[i] < smallestDistanceFromCenterAverage) {
            smallestDistanceFromCenterAverage = distanceFromCenterAverages[i];
            bestTestCenterHz = testCenterHz;
            channelBins = channelBinsArray[i];
        }
    }
    logDebug() << "bestTestCenterHz:" << bestTestCenterHz << "channelBins:" << channelBins;

    if (bestTestCenterHz != 0) {
        for (size_t i=0; i<size(); i++) {
            (*this)[i].oneBasedChannelBin = channelBins[i] + 1;
        }
        bestCenterHz = bestTestCenterHz;
        return false;
    }

    return bestTestCenterHz != 0;
}

std::string TagDatabase::detectorConfigFileName(const ExtTagInfo_t& extTagInfo, bool secondaryChannel) const
{
    return formatString("%s/detector.%d.config", getenv("HOME"), extTagInfo.tagInfo.id + (secondaryChannel ? 1 : 0));
}

bool TagDatabase::_writeDetectorConfig(const ExtTagInfo_t& extTagInfo, bool secondaryChannel) const
{
    const char* homePath = getenv("HOME");

    std::string configPath  = detectorConfigFileName(extTagInfo, secondaryChannel);

    logDebug() << "_writeDetectorConfig" << configPath;

    FILE* fp = fopen(configPath.c_str(), "w");
    if (fp == NULL) {
        logError() << "_writeDetectorConfig fopen failed -" << strerror(errno);
        return false;
    }

    int     secondaryChannelIncrement   = secondaryChannel ? 1 : 0;
    auto    tagId                       = extTagInfo.tagInfo.id + secondaryChannelIncrement;
    double  freqMHz                     = double(extTagInfo.tagInfo.frequency_hz) / 1000000.0;
    auto    tip_msecs                   = secondaryChannel ? extTagInfo.tagInfo.intra_pulse2_msecs : extTagInfo.tagInfo.intra_pulse1_msecs;
    //auto    portData                    = 20000 + ((extTagInfo.oneBasedChannelBin - 1) * 2) + secondaryChannelIncrement;
    auto    portData                    = 20000 + secondaryChannelIncrement;

    fprintf(fp, "##################################################\n");
    fprintf(fp, "ID:\t%d\n",                                tagId);
    fprintf(fp, "channelCenterFreqMHz:\t%f\n",              freqMHz);
    fprintf(fp, "ipData:\t127.0.0.1\n");
    fprintf(fp, "portData:\t%d\n",                          portData);
    fprintf(fp, "Fs:\t3750\n");
    fprintf(fp, "tagFreqMHz:\t%f\n",                        freqMHz);
    fprintf(fp, "tp:\t0.015\n");
    fprintf(fp, "tip:\t%f\n",                               double(tip_msecs) / 1000.0);
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
    for (const ExtTagInfo_t& extTagInfo : *this) {
        _writeDetectorConfig(extTagInfo, false);
        if (extTagInfo.tagInfo.intra_pulse2_msecs != 0) {
            _writeDetectorConfig(extTagInfo, true);
        }
    }

    return true;
}

std::string TagDatabase::channelizerCommandLine() const
{
    std::string commandLine;

    for (const ExtTagInfo_t& extTagInfo : *this) {
        commandLine += formatString("%d ", extTagInfo.oneBasedChannelBin * (extTagInfo.tagInfo.intra_pulse2_msecs != 0 ? -1 : 1));
    }

    return commandLine;
}
