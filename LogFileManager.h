#pragma once

#include <string>

class LogFileManager
{
public:
	static LogFileManager* instance();

	void 		detectorsStarted();
	void 		detectorsStopped();

	std::string filename(const char* root, const char* extension);
	std::string logDir() const { return _logDir; }

private:
	LogFileManager();
	
	std::string _homeDir;
	std::string _logDir;

	static LogFileManager* 	_instance;
	static const char* 		_logDirPrefix;
};
