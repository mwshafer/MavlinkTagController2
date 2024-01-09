#include "log.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_GRAY "\x1b[37m"
#define ANSI_COLOR_RESET "\x1b[0m"

std::mutex LogDetailed::_logMutex;

LogDetailed::LogDetailed(const char* filename, int filenumber) 
    : _s                ()
    , _caller_filename  (filename)
    , _caller_filenumber(filenumber)
{

}

LogDetailed::~LogDetailed()
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

void set_color(LogColor LogColor)
{
    switch (LogColor) {
        case LogColor::Red:
            std::cout << ANSI_COLOR_RED;
            break;
        case LogColor::Green:
            std::cout << ANSI_COLOR_GREEN;
            break;
        case LogColor::Yellow:
            std::cout << ANSI_COLOR_YELLOW;
            break;
        case LogColor::Blue:
            std::cout << ANSI_COLOR_BLUE;
            break;
        case LogColor::Gray:
            std::cout << ANSI_COLOR_GRAY;
            break;
        case LogColor::Reset:
            std::cout << ANSI_COLOR_RESET;
            break;
    }
}
