#include "MonitoredProcess.h"
#include "sendStatusText.h"
#include "log.h"

#include <string>
#include <iostream>
#include <thread>
#include <filesystem>

MonitoredProcess::MonitoredProcess(
		MavlinkOutgoingMessageQueue& 	outgoingMessageQueue, 
		const char* 					name, 
		const char* 					command, 
		const char* 					logPath, 
		IntermediatePipeType			intermediatePipeType,
		bp::pipe* 						intermediatePipe)
	: _outgoingMessageQueue (outgoingMessageQueue)
	, _name					(name)
	, _command				(command)
	, _logPath				(logPath)
	, _intermediatePipeType	(intermediatePipeType)
	, _intermediatePipe		(intermediatePipe)
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
	logDebug() << "MonitoredProcess::stop _childProcess:_childProcess.running" 
		<< _childProcess
		<< (_childProcess ? _childProcess->running() : false);

	if (_childProcess) {
		_terminated = true;
		_childProcess->terminate();
	}
}

void MonitoredProcess::_run(void)
{
	std::string statusStr("Process start: ");
	statusStr.append(_name);

	logInfo() << statusStr << "'" << _command.c_str() << "' >" << _logPath.c_str();
	sendStatusText(_outgoingMessageQueue, statusStr.c_str());

	std::filesystem::remove(_logPath);

	try {
		switch (_intermediatePipeType ) {
			case NoPipe:
				_childProcess = new bp::child(_command.c_str(), bp::std_out > _logPath, bp::std_err > _logPath);
				break;
			case InputPipe:
				_childProcess = new bp::child(_command.c_str(), bp::std_in < *_intermediatePipe, bp::std_out > _logPath, bp::std_err > _logPath);
				break;
			case OutputPipe:
				_childProcess = new bp::child(_command.c_str(), bp::std_out > *_intermediatePipe, bp::std_err > _logPath);
				break;
		}
	} catch(bp::process_error& e) {
		logError() << "MonitoredProcess::run boost::process:child threw process_error exception -" << e.what();
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

	_terminated = false;

	logError() << statusStr;
	sendStatusText(_outgoingMessageQueue, statusStr.c_str(), result == 0 ? MAV_SEVERITY_INFO : MAV_SEVERITY_ERROR);

    delete this;   
}