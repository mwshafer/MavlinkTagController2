#pragma once

#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

#include <string>
#include <thread>
#include <chrono>

#include <boost/process/child.hpp>

using namespace mavsdk;

class MonitoredProcess
{
public:
	MonitoredProcess(MavlinkPassthrough& mavlinkPassthrough, const char* name, const char* command, const char* logPath, bool restart);

	void start 	(void);
	void stop	(void);

private:
	void _run(void);

	MavlinkPassthrough&		_mavlinkPassthrough;
	std::string				_name;
	std::string 			_command;
	std::string				_logPath;
	bool					_restart 		= false;
	std::thread*			_thread			= NULL;
	boost::process::child*	_childProcess 	= NULL;
	bool					_terminated		= false;
};
