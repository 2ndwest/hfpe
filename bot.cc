#include <libtouchstone.h>
#include <cstdio>
#include "utils.h"

const char* COOKIE_FILE = "cookies.txt";

int main() {
    if (!std::getenv("KERB") || !std::getenv("KERB_PASSWORD") || !std::getenv("MIT_ID")) {
        printf("Some key environment variables are not set!\n");
        return 1;
    }

    auto session = libtouchstone::session("cookies.txt");

    wait_until_time(7, 59, "Waiting for 7:58am...");

    printf("Warming up cookies...\n");
    libtouchstone::authenticate(
        session,
        "https://eduapps.mit.edu/mitpe/student/registration/home",
        std::getenv("KERB"),
        std::getenv("KERB_PASSWORD"),
        {COOKIE_FILE, true, true}
    );

    wait_until_time(8, 0, "Waiting for 8am...");

    auto section_list_resp = libtouchstone::authenticate(
        session,
        "https://eduapps.mit.edu/mitpe/student/registration/sectionList",
        std::getenv("KERB"),
        std::getenv("KERB_PASSWORD"),
        {COOKIE_FILE, true, true}
    );

    // TODO parse
    std::string section_id;

    session.SetUrl(cpr::Url{"https://eduapps.mit.edu/mitpe/student/registration/create"});
    session.SetHeader(cpr::Header{
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Origin", "https://eduapps.mit.edu"},
        {"Referer", "https://eduapps.mit.edu/mitpe/student/registration/section?sectionId=" + section_id}
    });
    session.SetBody(cpr::Body{"sectionId=" + section_id + "&mitId=" + std::getenv("MIT_ID") + "&wf="});
    auto register_resp = session.Post();

    printf("Registration response: %ld\n", register_resp.status_code);
    printf("%s\n", register_resp.text.c_str());

    return 0;
}
