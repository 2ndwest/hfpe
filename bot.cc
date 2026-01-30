#include <libtouchstone.h>
#include <cstdio>
#include "utils.h"

const char* COOKIE_FILE = "cookies.txt";

int main() {
    if (!std::getenv("KERB") || !std::getenv("KERB_PASSWORD")) {
        printf("KERB and KERB_PASSWORD must be set\n");
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

    printf("Got %zu bytes from %s\n%s\n", section_list_resp.text.size(), section_list_resp.url.str().c_str(), section_list_resp.text.c_str());
    return 0;
}
