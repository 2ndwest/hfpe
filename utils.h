#pragma once

#include <cstdio>
#include <ctime>
#include <thread>
#include <chrono>
#include <random>
#include <cpr/cpr.h>

struct RetryConfig {
    int delay_ms = 100;     // Fixed delay between retries
    int jitter_ms = 30;     // Random jitter (±jitter_ms)
};

// Calculate delay with random jitter
inline int calculate_delay(const RetryConfig& config) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(-config.jitter_ms, config.jitter_ms);
    return config.delay_ms + dis(gen);
}

// Retry wrapper for HTTP requests (retries forever until success)
// Takes a function that performs the request and returns a cpr::Response
template<typename RequestFunc>
cpr::Response retry_request(RequestFunc request_func, const char* request_name, const RetryConfig& config = RetryConfig{}) {
    cpr::Response resp;
    int attempt = 0;

    while (true) {
        if (attempt > 0) {
            int delay = calculate_delay(config);
            printf("[INFO] %s: Attempt %d after %dms delay...\n",
                   request_name, attempt + 1, delay);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }

        resp = request_func();

        if (resp.status_code == 0) {
            printf("[WARN] %s: Network error - %s\n", request_name, resp.error.message.c_str());
        } else if (resp.status_code >= 500) {
            printf("[WARN] %s: Server error %ld\n", request_name, resp.status_code);
        } else if (resp.status_code == 429) {
            printf("[WARN] %s: Rate limited (429)\n", request_name);
        }

        if (!resp.error && resp.status_code == 200) {
            if (attempt > 0) printf("[INFO] %s: Succeeded on attempt %d with status %ld\n", request_name, attempt + 1, resp.status_code);
            else printf("[INFO] %s: Succeeded on first attempt with status %ld\n", request_name, resp.status_code);
            return resp;
        }

        ++attempt;
    }
}

inline std::pair<int, int> minutes_before(int hour, int min, int n) {
    int total = hour * 60 + min - n;
    if (total < 0) total += 24 * 60;
    return {total / 60, total % 60};
}

// Spin until the target time (target_hour:target_minute) is reached.
// NOTE: If the target hour has already passed, waits until the next day.
inline void wait_until_time(int target_hour, int target_minute, const char* message) {
    while (true) {
        time_t now = time(nullptr);
        struct tm* local = localtime(&now);
        if (local->tm_hour == target_hour && local->tm_min >= target_minute) break;
        printf("%s (target: %02d:%02d, now: %02d:%02d:%02d)\n",
               message, target_hour, target_minute,
               local->tm_hour, local->tm_min, local->tm_sec);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}
