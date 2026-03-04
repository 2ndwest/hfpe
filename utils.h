#pragma once

#include <atomic>
#include <chrono>
#include <cpr/cpr.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <random>
#include <sys/time.h>
#include <thread>

// Timestamped logging — every line gets HH:MM:SS.mmm
__attribute__((format(printf, 1, 2))) inline void tlog(const char *fmt, ...) {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  struct tm *local = localtime(&tv.tv_sec);
  fprintf(stdout, "[%02d:%02d:%02d.%03d] ", local->tm_hour, local->tm_min,
          local->tm_sec, (int)(tv.tv_usec / 1000));
  va_list args;
  va_start(args, fmt);
  vfprintf(stdout, fmt, args);
  va_end(args);
  fflush(stdout);
}

// Milliseconds since an arbitrary epoch, for measuring durations
inline long long now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct RetryConfig {
  int delay_ms = 100; // Fixed delay between retries
  int jitter_ms = 30; // Random jitter (±jitter_ms)
};

// Calculate delay with random jitter
inline int calculate_delay(const RetryConfig &config) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(-config.jitter_ms, config.jitter_ms);
  return config.delay_ms + dis(gen);
}

// Retry wrapper for HTTP requests (retries forever until success or cancel)
// Takes a function that performs the request and returns a cpr::Response.
// If cancel is non-null and becomes true, returns the last response
// immediately.
template <typename RequestFunc>
cpr::Response retry_request(RequestFunc request_func, const char *request_name,
                            const RetryConfig &config = RetryConfig{},
                            std::atomic<bool> *cancel = nullptr) {
  cpr::Response resp;
  int attempt = 0;
  long long first_attempt_ms = now_ms();

  while (true) {
    if (cancel && cancel->load()) {
      tlog("[INFO] %s: Cancelled after %d attempts (%lldms total)\n",
           request_name, attempt, now_ms() - first_attempt_ms);
      return resp;
    }

    if (attempt > 0 && config.delay_ms > 0) {
      int delay = calculate_delay(config);
      tlog("[INFO] %s: Attempt %d after %dms delay...\n", request_name,
           attempt + 1, delay);
      std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    } else if (attempt > 0) {
      tlog("[INFO] %s: Attempt %d (immediate retry)...\n", request_name,
           attempt + 1);
    }

    long long req_start = now_ms();
    resp = request_func();
    long long req_duration = now_ms() - req_start;

    if (cancel && cancel->load()) {
      tlog("[INFO] %s: Cancelled after %d attempts (%lldms total)\n",
           request_name, attempt + 1, now_ms() - first_attempt_ms);
      return resp;
    }

    if (resp.status_code == 0) {
      tlog("[WARN] %s: Network error after %lldms - %s\n", request_name,
           req_duration, resp.error.message.c_str());
    } else if (resp.status_code >= 500) {
      tlog("[WARN] %s: Server error %ld after %lldms\n", request_name,
           resp.status_code, req_duration);
    } else if (resp.status_code == 429) {
      tlog("[WARN] %s: Rate limited (429) after %lldms\n", request_name,
           req_duration);
    } else if (resp.status_code != 200) {
      tlog("[WARN] %s: Unexpected status %ld after %lldms\n", request_name,
           resp.status_code, req_duration);
    }

    if (!resp.error && resp.status_code == 200) {
      tlog(
          "[INFO] %s: Succeeded on attempt %d (%lldms request, %lldms total)\n",
          request_name, attempt + 1, req_duration, now_ms() - first_attempt_ms);
      return resp;
    }

    ++attempt;
  }
}

inline std::pair<int, int> minutes_before(int hour, int min, int n) {
  int total = hour * 60 + min - n;
  if (total < 0)
    total += 24 * 60;
  return {total / 60, total % 60};
}

// Spin until the target time (target_hour:target_minute) is reached.
// NOTE: If the target hour has already passed, waits until the next day.
inline void wait_until_time(int target_hour, int target_minute,
                            const char *message) {
  while (true) {
    time_t now = time(nullptr);
    struct tm *local = localtime(&now);
    if (local->tm_hour == target_hour && local->tm_min >= target_minute)
      break;
    if (local->tm_sec % 55 == 0) {
      tlog("%s (target: %02d:%02d, now: %02d:%02d:%02d)\n", message,
           target_hour, target_minute, local->tm_hour, local->tm_min,
           local->tm_sec);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}
