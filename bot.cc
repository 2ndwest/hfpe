#include "sectionlist.h"
#include "utils.h"
#include <atomic>
#include <cstdio>
#include <libtouchstone.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

const char *COOKIE_FILE = "cookies.txt";

const auto LIBTOUCHSTONE_OPTS =
    libtouchstone::AuthOptions{COOKIE_FILE, true, true};

// No delay between retries — long timeout keeps us in the TCP queue.
// Retry immediately only if the server actively rejects us.
const RetryConfig THREADED_RETRY{.delay_ms = 0, .jitter_ms = 0};

const int TIMEOUT_MS = 120000; // 120 seconds — stay in the TCP buffer
const int NUM_THREADS = 16;

std::string get_base_url() {
  const char *url = std::getenv("PE_BASE_URL");
  return url ? url : "https://eduapps.mit.edu";
}

std::pair<int, int> get_registration_time() {
  const char *t = std::getenv("PE_REGISTRATION_TIME");
  int hour = 8, min = 0;
  if (t)
    sscanf(t, "%d:%d", &hour, &min);
  return {hour, min};
}

int main() {
  char *kerb = std::getenv("KERB");
  char *kerb_password = std::getenv("KERB_PASSWORD");
  char *mit_id = std::getenv("MIT_ID");
  char *pe_section_name = std::getenv("PE_SECTION_NAME");
  if (!kerb || !kerb_password || !mit_id || !pe_section_name) {
    tlog("[ERROR] Some key environment variables are not set!\n");
    return 1;
  }

  std::string base_url = get_base_url();
  auto [reg_hour, reg_min] = get_registration_time();
  auto [warmup_hour, warmup_min] = minutes_before(reg_hour, reg_min, 5);
  int num_threads = NUM_THREADS;

  tlog("Initializing bot for %s targeting %s (%d threads, %ds timeout)\n",
       kerb, pe_section_name, num_threads, TIMEOUT_MS / 1000);
  tlog("Base URL: %s\n", base_url.c_str());
  tlog("Registration time: %02d:%02d, Warmup time: %02d:%02d\n", reg_hour,
       reg_min, warmup_hour, warmup_min);

  // Set environment to Eastern Time
  setenv("TZ", "America/New_York", 1);
  tzset();

  tlog("Verifying credentials...\n");
  {
    long long auth_start = now_ms();
    auto init_session = libtouchstone::session(COOKIE_FILE);
    auto init_resp = libtouchstone::authenticate(
        init_session, (base_url + "/mitpe/student/registration/home").c_str(),
        kerb, kerb_password, LIBTOUCHSTONE_OPTS);
    if (init_resp.status_code != 200 || init_resp.error) {
      tlog("[ERROR] Initial auth failed: status %ld, error: %s (%lldms)\n",
           init_resp.status_code, init_resp.error.message.c_str(),
           now_ms() - auth_start);
      return 1;
    }
    tlog("[INFO] Credentials OK (%lldms)\n", now_ms() - auth_start);
  } // init_session goes out of scope, saving cookies to cookies.txt

  auto session = libtouchstone::session(COOKIE_FILE);
  session.SetTimeout(cpr::Timeout{TIMEOUT_MS});

  wait_until_time(warmup_hour, warmup_min, "Waiting for warmup...");

  tlog("Warming up cookies...\n");
  {
    long long warmup_start = now_ms();
    auto warmup_resp = libtouchstone::authenticate(
        session, (base_url + "/mitpe/student/registration/home").c_str(), kerb,
        kerb_password, LIBTOUCHSTONE_OPTS);
    if (warmup_resp.status_code != 200 || warmup_resp.error) {
      tlog("[WARN] Cookie warmup failed: status %ld, error %d (%lldms)\n",
           warmup_resp.status_code, static_cast<int>(warmup_resp.error.code),
           now_ms() - warmup_start);
    } else {
      tlog("[INFO] Cookie warmup OK (%lldms)\n", now_ms() - warmup_start);
    }
  }

  wait_until_time(reg_hour, reg_min, "Waiting for registration...");

  long long race_start = now_ms();

  // --- Shared state for worker threads ---
  std::atomic<bool> got_section_id{false};
  std::string shared_section_id;
  std::mutex section_id_mutex;

  std::atomic<bool> registered{false};
  std::string registration_response;
  std::mutex registration_mutex;

  // Copy env vars into std::strings so threads don't race on getenv
  std::string s_kerb = kerb;
  std::string s_kerb_password = kerb_password;
  std::string s_mit_id = mit_id;
  std::string s_pe_section_name = pe_section_name;

  auto worker = [&](int tid) {
    char tag[16];
    snprintf(tag, sizeof(tag), "T%d", tid);
    long long thread_start = now_ms();

    tlog("[%s] Thread started\n", tag);

    // Each thread gets its own session (own CURL handle + cookies)
    auto tsession = libtouchstone::session(COOKIE_FILE);
    tsession.SetTimeout(cpr::Timeout{TIMEOUT_MS});

    // Phase 1: Fetch section list
    char fetch_tag[32];
    snprintf(fetch_tag, sizeof(fetch_tag), "T%d_FETCH", tid);

    auto section_resp = retry_request(
        [&]() {
          return libtouchstone::authenticate(
              tsession,
              (base_url + "/mitpe/student/registration/sectionList").c_str(),
              s_kerb.c_str(), s_kerb_password.c_str(), LIBTOUCHSTONE_OPTS);
        },
        fetch_tag, THREADED_RETRY, &got_section_id);

    // If we got a good response and nobody else found the ID yet, extract it
    if (!got_section_id.load() && !section_resp.error &&
        section_resp.status_code == 200) {
      auto maybe_id = find_section_id(section_resp.text, s_pe_section_name);
      if (maybe_id) {
        std::lock_guard<std::mutex> lock(section_id_mutex);
        if (!got_section_id.load()) {
          shared_section_id = *maybe_id;
          got_section_id.store(true);
          tlog("[%s] Found section ID: %s (%lldms since launch)\n", tag,
               shared_section_id.c_str(), now_ms() - race_start);
        }
      } else {
        tlog("[WARN] [%s] Got 200 but section name '%s' not found in HTML "
             "(%zu bytes)\n",
             tag, s_pe_section_name.c_str(), section_resp.text.size());
      }
    }

    // Wait for section ID (another thread may have found it)
    while (!got_section_id.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::string section_id;
    {
      std::lock_guard<std::mutex> lock(section_id_mutex);
      section_id = shared_section_id;
    }

    tlog("[%s] Starting registration POST for section %s\n", tag,
         section_id.c_str());

    // Phase 2: Submit registration
    tsession.SetUrl(
        cpr::Url{base_url + "/mitpe/student/registration/create"});
    tsession.SetHeader(
        cpr::Header{{"Content-Type", "application/x-www-form-urlencoded"},
                     {"Origin", base_url},
                     {"Referer", base_url +
                                     "/mitpe/student/registration/section"
                                     "?sectionId=" +
                                     section_id}});
    tsession.SetBody(cpr::Body{"sectionId=" + section_id +
                                "&mitId=" + s_mit_id + "&wf="});

    char reg_tag[32];
    snprintf(reg_tag, sizeof(reg_tag), "T%d_REGISTER", tid);

    auto reg_resp = retry_request([&]() { return tsession.Post(); }, reg_tag,
                                  THREADED_RETRY, &registered);

    if (!registered.load() && !reg_resp.error && reg_resp.status_code == 200) {
      std::lock_guard<std::mutex> lock(registration_mutex);
      if (!registered.load()) {
        registered.store(true);
        registration_response = reg_resp.text;
        tlog("[%s] REGISTERED! (%lldms since launch)\n", tag,
             now_ms() - race_start);
      }
    }

    tlog("[%s] Thread exiting (%lldms lifetime)\n", tag,
         now_ms() - thread_start);
  };

  tlog("[INFO] Launching %d threads...\n", num_threads);

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(worker, i);
  }
  for (auto &t : threads) {
    t.join();
  }

  tlog("[INFO] All threads joined (%lldms total race time)\n",
       now_ms() - race_start);

  if (registered.load()) {
    tlog("\nRegistration successful! Response:\n%s\n",
         registration_response.c_str());
    return 0;
  }

  // This shouldn't happen since threads retry forever, but just in case
  tlog("\n[ERROR] All threads exited without successful registration.\n");
  return 1;
}
