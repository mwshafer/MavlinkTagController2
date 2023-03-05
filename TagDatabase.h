#pragma once

#include <vector>
#include <cstdint>
#include <string>

#include "TunnelProtocol.h"

typedef struct {
    TunnelProtocol::TagInfo_t   tagInfo;
    uint32_t                    oneBasedChannelBin;
} ExtTagInfo_t;

class TagDatabase : public std::vector<ExtTagInfo_t>
{
public:
    TagDatabase() = default;

    void        addTag                  (const TunnelProtocol::TagInfo_t& tagInfo);
    bool        channelizerTuner        (
                    const uint32_t              sampleRateHz, 
                    const uint32_t              nChannels, 
                    uint32_t&                   bestCenterHz);
    std::string detectorConfigFileName  (const ExtTagInfo_t& extTagInfo, bool secondaryChannel) const;
    bool        writeDetectorConfigs    () const;
    std::string channelizerCommandLine  () const;

private:
    bool        _writeDetectorConfig    (const ExtTagInfo_t& extTagInfo, bool secondaryChannel) const;
};
