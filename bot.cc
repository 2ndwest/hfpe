#include <libtouchstone.h>
#include <cstdio>

const char* COOKIE_FILE = "cookies.txt";

int main() {
    if (std::getenv("KERB") == nullptr || std::getenv("KERB_PASSWORD") == nullptr) {
        printf("KERB and KERB_PASSWORD must be set\n");
        return 1;
    }

    auto session = libtouchstone::session("cookies.txt");

    auto response = libtouchstone::authenticate(
        session,
        "https://eduapps.mit.edu/mitpe/student/registration/sectionList",
        std::getenv("KERB"),
        std::getenv("KERB_PASSWORD"),
        {COOKIE_FILE, true, true}
    );

    printf("Got %zu bytes from %s\n", response.text.size(), response.url.str().c_str());
    return 0;
}
