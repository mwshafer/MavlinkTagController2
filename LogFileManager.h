#pragma once

#include <string>

class LogFileManager
{
public:
	static LogFileManager* instance();

	void 		detectorsStarted();
	std::string filename(const char* root, const char* extension);
	std::string logDir() const { return _logDir; }
	int 		detectorStartIndex() const { return _detectorStartIndex; }

private:
	LogFileManager();
	
	int 		_detectorStartIndex = 0;
	std::string _homeDir;
	std::string _logDir;

	static LogFileManager* 	_instance;
	static const char* 		_logDirPrefix;
};
