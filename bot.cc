#include <libtouchstone.h>
#include <cstdio>
#include "utils.h"

const auto opts = libtouchstone::AuthOptions{"cookies.txt", true, true};

const RetryConfig AGGRESSIVE_RETRY{
    .delay_ms = 250,
    .jitter_ms = 30
};

int main() {
    if (!std::getenv("KERB") || !std::getenv("KERB_PASSWORD") || !std::getenv("MIT_ID")) {
        printf("[ERROR] Some key environment variables are not set!\n");
        return 1;
    }

    auto session = libtouchstone::session("cookies.txt");

    wait_until_time(7, 55, "Waiting for 7:55am...");

    printf("Warming up cookies...\n");
    auto warmup_resp = libtouchstone::authenticate(session,
        "https://eduapps.mit.edu/mitpe/student/registration/home",
        std::getenv("KERB"), std::getenv("KERB_PASSWORD"), opts
    );

    if (warmup_resp.status_code != 200 || warmup_resp.error) {
        printf("[WARN] Cookie warmup returned status %ld and had error code %d\n",
               warmup_resp.status_code,
               static_cast<int>(warmup_resp.error.code));
    }

    wait_until_time(8, 0, "Waiting for 8am...");

    auto section_list_resp = retry_request([&]() {
        // We do an authenticate call here just in case the cookies somehow went stale
        // in the meantime or another SSO redirect is needed for some reason. After this,
        // subsequent requests shouldn't require authentication.
        return libtouchstone::authenticate(session,
            "https://eduapps.mit.edu/mitpe/student/registration/sectionList",
            std::getenv("KERB"), std::getenv("KERB_PASSWORD"), opts
        );
    }, "FETCH_SECTION_LIST", AGGRESSIVE_RETRY);

    // TODO parse
    std::string section_id;

    session.SetUrl(cpr::Url{"https://eduapps.mit.edu/mitpe/student/registration/create"});
    session.SetHeader(cpr::Header{
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Origin", "https://eduapps.mit.edu"},
        {"Referer", "https://eduapps.mit.edu/mitpe/student/registration/section?sectionId=" + section_id}
    });
    session.SetBody(cpr::Body{"sectionId=" + section_id + "&mitId=" + std::getenv("MIT_ID") + "&wf="});

    auto register_resp = retry_request([&]() {
        return session.Post();
    }, "SUBMIT_REGISTRATION", AGGRESSIVE_RETRY);


    printf("Success (status %ld)! Response:\n%s\n", register_resp.status_code, register_resp.text.c_str());

    return 0;
}
