#include "core/localization.h"
#include <cstdio>
#include <string>

using namespace dc;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            ++failures;
            std::printf("FAIL: %s\n", message);
        }
    };

    check(localizeText("HUNT MODE", Language::English) == "HUNT MODE", "English remains unchanged");
    check(localizeText("HUNT MODE", Language::Russian) == u8"РЕЖИМ ОХОТЫ", "Russian menu translation");
    check(localizeText("HUNT MODE", Language::Kazakh) == u8"АҢШЫЛЫҚ РЕЖІМІ", "Kazakh menu translation");
    check(localizeText("KAIN WINS", Language::Russian) == u8"KAIN побеждает", "Russian dynamic winner label");
    check(localizeText("KAIN WINS", Language::Kazakh) == u8"KAIN жеңді", "Kazakh dynamic winner label");
    check(localizeText("MOVE  W A S D / LEFT STICK      LIGHT J", Language::Russian).find(u8"ДВИЖЕНИЕ") != std::string::npos,
        "Russian control hints");
    check(localizeText("A brawler whose Stand beats in time with his heart. Close the distance and never stop punching.",
        Language::Kazakh).find(u8"Жүрегімен") != std::string::npos, "Kazakh character bio");
    check(std::string(languageName(Language::English)) == "ENGLISH", "English language label");
    check(std::string(languageName(Language::Russian)) == u8"РУССКИЙ", "Russian language label");
    check(std::string(languageName(Language::Kazakh)) == u8"ҚАЗАҚША", "Kazakh language label");

    std::printf("Localization: %d failures\n", failures);
    return failures ? 1 : 0;
}
