#pragma once

#include <string>
#include <thread>
#include <chrono>
#include <memory>

#include <boost/process.hpp>

namespace bp = boost::process;

class MavlinkSystem;

class MonitoredProcess
{
public:
	enum IntermediatePipeType {
		NoPipe,
		InputPipe,
		OutputPipe,
	};

	MonitoredProcess(
		MavlinkSystem*					mavlink,
		const char* 					name, 
		const char* 					command, 
		const char* 					logPath, 
		IntermediatePipeType			intermediatePipeType,
		bp::pipe* 						intermediatePipe);

	void start 	(void);
	void stop	(void);

private:
	void _run(void);

	MavlinkSystem*					_mavlink;
	std::string						_name;
	std::string 					_command;
	std::string						_logPath;
	std::thread*					_thread			= NULL;
	boost::process::child*			_childProcess 	= NULL;
	bool							_terminated		= false;
	IntermediatePipeType			_intermediatePipeType;
	bp::pipe*						_intermediatePipe;
};
