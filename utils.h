#pragma once

#include <cstdio>
#include <ctime>
#include <thread>
#include <chrono>

inline void wait_until_time(int target_hour, int target_minute, const char* message) {
    while (true) {
        time_t now = time(nullptr);
        struct tm* local = localtime(&now);
        if (local->tm_hour == target_hour && local->tm_min >= target_minute) break;
        if (local->tm_hour > target_hour) break;  // Already past target time
        printf("%s (current time: %02d:%02d:%02d)\n", message, local->tm_hour, local->tm_min, local->tm_sec);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
