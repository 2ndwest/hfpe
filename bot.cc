#include <libtouchstone.h>
#include <cstdio>

const char* COOKIE_FILE = "cookies.txt";

void wait_until_time(int target_hour, int target_minute, const char* message) {
    while (true) {
        time_t now = time(nullptr);
        struct tm* local = localtime(&now);
        if (local->tm_hour == target_hour && local->tm_min >= target_minute) break;
        if (local->tm_hour > target_hour) break;  // Already past target time
        printf("%s (current time: %02d:%02d:%02d)\n", message, local->tm_hour, local->tm_min, local->tm_sec);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

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
        "https://eduapps.mit.edu/mitpe/student/registration/sectionList",
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
