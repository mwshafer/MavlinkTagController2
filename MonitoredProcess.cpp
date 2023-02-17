#include "MonitoredProcess.h"
#include "sendStatusText.h"

#include <string>
#include <iostream>
#include <thread>

#include <boost/process/child.hpp>
#include <boost/process/io.hpp>

namespace bp = boost::process;

MonitoredProcess::MonitoredProcess(MavlinkPassthrough& mavlinkPassthrough, const char* name, const char* command, const char* logPath, bool restart)
	: _mavlinkPassthrough 	(mavlinkPassthrough)
	, _name					(name)
	, _command				(command)
	, _logPath				(logPath)
	, _restart				(restart)
{

}

void MonitoredProcess::start(void)
{
	// FIXME: We leak these thread objects
    _thread = new std::thread(&MonitoredProcess::_run, this);
    _thread->detach();
}

void MonitoredProcess::stop(void)
{
	std::cout << "MonitoredProcess::Stop _childProcess:_childProcess.running " 
		<< _childProcess << " "
		<< (_childProcess ? _childProcess->running() : false)
		<< std::endl;

	if (_childProcess) {
		_terminated = true;
		_childProcess->terminate();
	}
}

void MonitoredProcess::_run(void)
{
	bool startProcess = true;

	while (startProcess) {
		std::string statusStr("Process start: ");
		statusStr.append(_name);

	    std::cout << statusStr << "'" << _command.c_str() << "' >" << _logPath.c_str() << std::endl;
	    sendStatusText(_mavlinkPassthrough, statusStr.c_str());

		try {
	    	_childProcess = new bp::child(_command.c_str(), bp::std_out > _logPath);
		} catch(bp::process_error& e) {
			std::cout << "MonitoredProcess::run boost::process:child threw process_error exception - " << e.what() << std::endl;
			_terminated = true;
//		} catch(...) {
//			std::cout << "MonitoredProcess::run boost::process:child threw unknown exception" << std::endl;
//			_terminated = true;
		}

		int result = 255;

		if (_childProcess) {
	    	_childProcess->wait();
	    	result = _childProcess->exit_code();
		}

	   	if (result == 0) {
	   		statusStr = "Process end: ";
	   	} else if (_terminated) {
	   		statusStr = "Process terminated: ";
			_restart = false;
	   	} else {
	   		char numStr[21];

	   		statusStr = "Process fail: ";
	   		sprintf(numStr, "%d", result);
	   		statusStr.append(numStr);
	   		statusStr.append(" ");
	   	}
	   	statusStr.append(_name);

	   	delete _childProcess;
	   	_childProcess = NULL;

	   	if (_restart) {
	   		statusStr.append("- Restarting");
	   	}
	   	_terminated = false;

	   	std::cout << statusStr << std::endl;
	    sendStatusText(_mavlinkPassthrough, statusStr.c_str(), result == 0 ? MAV_SEVERITY_INFO : MAV_SEVERITY_ERROR);

	    startProcess = _restart;
    }

    delete this;   
}