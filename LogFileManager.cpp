#include "LogFileManager.h"
#include "formatString.h"
#include "log.h"

#include <stdlib.h>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

namespace bf = boost::filesystem;
namespace bs = boost::system;

LogFileManager* LogFileManager::_instance       = nullptr;
const char* 	LogFileManager::_logDirPrefix   = "Logs";

LogFileManager* LogFileManager::instance()
{
    if (_instance == nullptr) {
        _instance = new LogFileManager();
    }
    return _instance;
}

LogFileManager::LogFileManager()
{
    _homeDir    = std::string(getenv("HOME"));
    _logDir     = formatString("%s/%s_1", _homeDir.c_str(), _logDirPrefix);
}

void LogFileManager::detectorsStarted()
{
    if (_detectorStartIndex++ == 0) {
        bs::error_code errorCode;
        
        for (int i=10; i>=1; i--) {
            auto oldName = formatString("%s/%s_%d", _homeDir.c_str(), _logDirPrefix, i);
            auto newName = formatString("%s/%s_%d", _homeDir.c_str(), _logDirPrefix, i + 1);
            bf::rename(oldName.c_str(), newName.c_str(), errorCode);
        }

        bf::create_directory(_logDir.c_str(), errorCode);
    }
}

std::string LogFileManager::filename(const char* root, const char* extension)
{
    return formatString("%s/%s.%d.%s", _logDir.c_str(), root, _detectorStartIndex, extension);
}
