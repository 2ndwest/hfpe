#include "sectionlist.h"
#include "utils.h"
#include <cstdio>
#include <fstream>
#include <libtouchstone.h>
#include <string>

const char* COOKIE_FILE = "cookies.txt";

const auto LIBTOUCHSTONE_OPTS =
    libtouchstone::AuthOptions{COOKIE_FILE, true, true};

const RetryConfig RETRY_CONFIG{.delay_ms = 250, .jitter_ms = 30};

std::string get_base_url() {
  const char *url = std::getenv("PE_BASE_URL");
  return url ? url : "https://eduapps.mit.edu";
}

std::pair<int, int> get_registration_time() {
  const char *t = std::getenv("PE_REGISTRATION_TIME");
  int hour = 8, min = 0;
  if (t) sscanf(t, "%d:%d", &hour, &min);
  return {hour, min};
}

int main() {
  char *kerb = std::getenv("KERB");
  char *kerb_password = std::getenv("KERB_PASSWORD");
  char *mit_id = std::getenv("MIT_ID");
  char *pe_section_name = std::getenv("PE_SECTION_NAME");
  if (!kerb || !kerb_password || !mit_id || !pe_section_name) {
    printf("[ERROR] Some key environment variables are not set!\n");
    return 1;
  }

  std::string base_url = get_base_url();
  auto [reg_hour, reg_min] = get_registration_time();
  auto [warmup_hour, warmup_min] = minutes_before(reg_hour, reg_min, 5);

  printf("Initializing bot for %s targeting %s\n", kerb, pe_section_name);

  // Set environment to Eastern Time
  setenv("TZ", "America/New_York", 1);
  tzset();

  printf("Verifying credentials...\n");
  {
    auto init_session = libtouchstone::session(COOKIE_FILE);
    auto init_resp = libtouchstone::authenticate(
        init_session, (base_url + "/mitpe/student/registration/home").c_str(),
        kerb, kerb_password, LIBTOUCHSTONE_OPTS);
    if (init_resp.status_code != 200 || init_resp.error) {
      fprintf(stderr, "[ERROR] Initial auth failed: status %ld, error: %s\n",
              init_resp.status_code, init_resp.error.message.c_str());
      return 1;
    }
  } // init_session goes out of scope, saving cookies to cookies.txt
  printf("[INFO] Credentials OK.\n");

  auto session = libtouchstone::session(COOKIE_FILE);
  session.SetTimeout(cpr::Timeout{10000}); // 10 second timeout

  wait_until_time(warmup_hour, warmup_min, "Waiting for warmup...");

  printf("Warming up cookies...\n");
  auto warmup_resp = libtouchstone::authenticate(
      session, (base_url + "/mitpe/student/registration/home").c_str(), kerb,
      kerb_password, LIBTOUCHSTONE_OPTS);
  if (warmup_resp.status_code != 200 || warmup_resp.error) {
    fprintf(stderr, "[WARN] Cookie warmup failed: status %ld, error %d\n",
            warmup_resp.status_code, static_cast<int>(warmup_resp.error.code));
  }

  wait_until_time(reg_hour, reg_min, "Waiting for registration...");

  auto section_list_resp = retry_request(
      [&]() {
        // We do an authenticate call here just in case the cookies somehow went
        // stale in the meantime or another SSO redirect is needed for some
        // reason. After this, subsequent requests shouldn't require
        // authentication.
        return libtouchstone::authenticate(
            session,
            (base_url + "/mitpe/student/registration/sectionList").c_str(),
            std::getenv("KERB"), std::getenv("KERB_PASSWORD"),
            LIBTOUCHSTONE_OPTS);
      },
      "FETCH_SECTION_LIST", RETRY_CONFIG);

  std::string section_list_html = section_list_resp.text;
  auto maybe_section_id =
      find_section_id(section_list_html, std::string(pe_section_name));
  if (!maybe_section_id) {
    fprintf(stderr, "Failed to find section name in section list response!\n");
    std::ofstream file("sectionlist_resp.html");
    if (file.is_open()) {
      file << section_list_html;
      file.close();
    } else {
      fprintf(stderr, "Failed to open sectionlist_resp.html");
    }
    return 1;
  }
  std::string section_id = *maybe_section_id;

  session.SetUrl(
      cpr::Url{base_url + "/mitpe/student/registration/create"});
  session.SetHeader(cpr::Header{
      {"Content-Type", "application/x-www-form-urlencoded"},
      {"Origin", base_url},
      {"Referer",
       base_url + "/mitpe/student/registration/section?sectionId=" +
           section_id}});
  session.SetBody(cpr::Body{"sectionId=" + section_id +
                            "&mitId=" + std::getenv("MIT_ID") + "&wf="});

  auto register_resp = retry_request([&]() { return session.Post(); },
                                     "SUBMIT_REGISTRATION", RETRY_CONFIG);

  printf("Success (status %ld)! Response:\n%s\n", register_resp.status_code,
         register_resp.text.c_str());

  return 0;
}
