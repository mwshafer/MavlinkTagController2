#include "log.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_GRAY "\x1b[37m"
#define ANSI_COLOR_RESET "\x1b[0m"

std::mutex LogDetailed::_logMutex;

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
