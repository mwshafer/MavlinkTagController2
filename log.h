#pragma once

#include <sstream>
#include <vector>
#include <iostream>
#include <ctime>
#include <mutex>
#include <functional>

// Remove path and extract only filename.
#define FILENAME \
    (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define call_user_callback(...) call_user_callback_located(FILENAME, __LINE__, __VA_ARGS__)

#define logDebug() LogDebugDetailed(FILENAME, __LINE__)
#define logInfo() LogInfoDetailed(FILENAME, __LINE__)
#define logWarn() LogWarnDetailed(FILENAME, __LINE__)
#define logError() LogErrDetailed(FILENAME, __LINE__)

enum class LogColor { Red, Green, Yellow, Blue, Gray, Reset };
enum class LogLevel : int { Debug = 0, Info = 1, Warn = 2, Err = 3 };

void set_color(LogColor LogColor);

class LogDetailed {
public:
    LogDetailed(const char* filename, int filenumber) :
        _s(),
        _caller_filename(filename),
        _caller_filenumber(filenumber)
    {}

    template<typename T> LogDetailed& operator<<(const T& x)
    {
        _s << x << " ";
        return *this;
    }

    template<typename T> LogDetailed& operator<<(const std::vector<T>& vector)
    {
        for (auto value : vector) {
            _s << value << ", ";
        }   
        return *this;
    }

    virtual ~LogDetailed()
    {

        switch (_log_level) {
            case LogLevel::Debug:
                set_color(LogColor::Green);
                break;
            case LogLevel::Info:
                set_color(LogColor::Blue);
                break;
            case LogLevel::Warn:
                set_color(LogColor::Yellow);
                break;
            case LogLevel::Err:
                set_color(LogColor::Red);
                break;
        }

        _logMutex.lock();

        // Time output taken from:
        // https://stackoverflow.com/questions/16357999#answer-16358264
        time_t rawtime;
        time(&rawtime);
        struct tm* timeinfo = localtime(&rawtime);
        char time_buffer[10]{}; // We need 8 characters + \0
        strftime(time_buffer, sizeof(time_buffer), "%I:%M:%S", timeinfo);
        std::cout << "[" << time_buffer;

        switch (_log_level) {
            case LogLevel::Debug:
                std::cout << "|D] ";
                break;
            case LogLevel::Info:
                std::cout << "|I] ";
                break;
            case LogLevel::Warn:
                std::cout << "|W] ";
                break;
            case LogLevel::Err:
                std::cout << "|E] ";
                break;
        }

        set_color(LogColor::Reset);

        std::cout << _s.str();
        std::cout << " (" << _caller_filename << ":" << std::dec << _caller_filenumber << ")";

        std::cout << std::endl;

        _logMutex.unlock();
    }

    LogDetailed(const LogDetailed&) = delete;
    void operator=(const LogDetailed&) = delete;

protected:
    LogLevel _log_level = LogLevel::Debug;

private:
    std::stringstream   _s;
    const char*         _caller_filename;
    int                 _caller_filenumber;

    static std::mutex   _logMutex;
};

class LogDebugDetailed : public LogDetailed {
public:
    LogDebugDetailed(const char* filename, int filenumber) : LogDetailed(filename, filenumber)
    {
        _log_level = LogLevel::Debug;
    }
};

class LogInfoDetailed : public LogDetailed {
public:
    LogInfoDetailed(const char* filename, int filenumber) : LogDetailed(filename, filenumber)
    {
        _log_level = LogLevel::Info;
    }
};

class LogWarnDetailed : public LogDetailed {
public:
    LogWarnDetailed(const char* filename, int filenumber) : LogDetailed(filename, filenumber)
    {
        _log_level = LogLevel::Warn;
    }
};

class LogErrDetailed : public LogDetailed {
public:
    LogErrDetailed(const char* filename, int filenumber) : LogDetailed(filename, filenumber)
    {
        _log_level = LogLevel::Err;
    }
};
