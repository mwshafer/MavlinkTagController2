#include "LogFileManager.h"
#include "formatString.h"

#include <stdlib.h>
#include <filesystem>

LogFileManager* LogFileManager::_instance = nullptr;

LogFileManager* LogFileManager::instance()
{
    if (_instance == nullptr) {
        _instance = new LogFileManager();
    }
    return _instance;
}

LogFileManager::LogFileManager()
{
    _logDir = std::string(getenv("HOME")) + "/Logs";
}

void LogFileManager::detectorsStarted()
{
    if (_detectorStartIndex++ == 0) {
        std::filesystem::remove_all(_logDir);
        std::filesystem::create_directory(_logDir);
    }
}

std::string LogFileManager::filename(const char* root, const char* extension)
{
    return formatString("%s/%s.%d.%s", _logDir.c_str(), root, _detectorStartIndex, extension);
}
