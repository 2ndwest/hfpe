#include "sectionlist.h"
#include "utils.h"
#include <cstdio>
#include <fstream>
#include <libtouchstone.h>
#include <string>

const auto LIBTOUCHSTONE_OPTS =
    libtouchstone::AuthOptions{"cookies.txt", true, true};

const RetryConfig RETRY_CONFIG{.delay_ms = 250, .jitter_ms = 30};

// Default: https://eduapps.mit.edu
// For testing: http://localhost:8080
std::string get_base_url() {
  const char *env_url = std::getenv("PE_BASE_URL");
  return env_url ? env_url : "https://eduapps.mit.edu";
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
  printf("Initializing bot for %s targeting %s (base URL: %s)\n", kerb,
         pe_section_name, base_url.c_str());

  auto session = libtouchstone::session("cookies.txt");

  // Set environment to Eastern Time
  setenv("TZ", "America/New_York", 1);
  tzset();

  wait_until_time(7, 55, "Waiting for 7:55am...");

  printf("Warming up cookies...\n");
  auto warmup_resp = libtouchstone::authenticate(
      session, (base_url + "/mitpe/student/registration/home").c_str(), kerb,
      kerb_password, LIBTOUCHSTONE_OPTS);

  if (warmup_resp.status_code != 200 || warmup_resp.error) {
    fprintf(stderr,
            "[WARN] Cookie warmup returned status %ld and had error code %d\n",
            warmup_resp.status_code, static_cast<int>(warmup_resp.error.code));
  }

  wait_until_time(8, 0, "Waiting for 8am...");

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
