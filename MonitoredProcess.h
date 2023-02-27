#pragma once

#include <mavsdk/plugins/mavlink_passthrough/mavlink_passthrough.h>

#include <string>
#include <thread>
#include <chrono>
#include <memory>

#include <boost/process.hpp>

using namespace mavsdk;

namespace bp = boost::process;

class MonitoredProcess
{
public:
	enum IntermediatePipeType {
		NoPipe,
		InputPipe,
		OutputPipe,
	};

	MonitoredProcess(
		MavlinkPassthrough& 		mavlinkPassthrough, 
		const char* 				name, 
		const char* 				command, 
		const char* 				logPath, 
		bool 						restart, 
		IntermediatePipeType		intermediatePipeType,
		std::shared_ptr<bp::pipe>& 	intermediatePipe);

	void start 	(void);
	void stop	(void);

private:
	void _run(void);

	MavlinkPassthrough&			_mavlinkPassthrough;
	std::string					_name;
	std::string 				_command;
	std::string					_logPath;
	bool						_restart 		= false;
	std::thread*				_thread			= NULL;
	boost::process::child*		_childProcess 	= NULL;
	bool						_terminated		= false;
	IntermediatePipeType		_intermediatePipeType;
	std::shared_ptr<bp::pipe>	_intermediatePipe;
	static bp::pipe				staticPipe;
};
