#pragma once

#include <chrono>

#define millis_now() uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count())
