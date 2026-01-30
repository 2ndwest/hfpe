#include <libtouchstone.h>
#include <cstdio>

const char* COOKIE_FILE = "cookies.txt";

int main() {
    if (!std::getenv("KERB") || !std::getenv("KERB_PASSWORD")) {
        printf("KERB and KERB_PASSWORD must be set\n");
        return 1;
    }

    auto session = libtouchstone::session("cookies.txt");

    // Spin loop until 8am
    while (true) {
        time_t now = time(nullptr);
        struct tm* local = localtime(&now);
        if (local->tm_hour == 8) break;
        printf("Waiting for 8am... (current time: %02d:%02d:%02d)\n", local->tm_hour, local->tm_min, local->tm_sec);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

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
