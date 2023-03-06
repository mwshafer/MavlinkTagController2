#pragma once

#include "MavlinkOutgoingMessageQueue.h"

#include <string>
#include <thread>
#include <chrono>
#include <memory>

#include <boost/process.hpp>

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
		MavlinkOutgoingMessageQueue& 	outgoingMessageQueue, 
		const char* 					name, 
		const char* 					command, 
		const char* 					logPath, 
		IntermediatePipeType			intermediatePipeType,
		std::shared_ptr<bp::pipe>& 		intermediatePipe);

	void start 	(void);
	void stop	(void);

private:
	void _run(void);

	MavlinkOutgoingMessageQueue&	_outgoingMessageQueue;
	std::string						_name;
	std::string 					_command;
	std::string						_logPath;
	std::thread*					_thread			= NULL;
	boost::process::child*			_childProcess 	= NULL;
	bool							_terminated		= false;
	IntermediatePipeType			_intermediatePipeType;
	std::shared_ptr<bp::pipe>		_intermediatePipe;
	static bp::pipe					staticPipe;
};
