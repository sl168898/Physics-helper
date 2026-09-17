#include "ProfileStore.h"
#include <cassert>
#include <chrono>
#include <limits>
#include <locale>

struct CommaDecimal : std::numpunct<char>
{
    char do_decimal_point() const override { return ','; }
};

int main()
{
    using namespace phial;
    Profile remembered{ true, { 1, 12.5f, 68 } };
    auto encoded = serializeProfile(remembered);
    assert(encoded);
    auto parsed = parseProfile(*encoded);
    assert(parsed && parsed.profile.enabled && parsed.profile.values == remembered.values);
    assert(!parseProfile("[WhitePhial]\nRememberAcrossSaves=1\nHotkey=68\n"));
    assert(!parseProfile("[WhitePhial]\nRememberAcrossSaves=1\nFullyReenchanted=1\nRefillHours=NaN\nHotkey=68\n"));
    assert(!parseProfile("[WhitePhial]\nRememberAcrossSaves=1\nFullyReenchanted=1\nRefillHours=12\nHotkey=9999\n"));
    assert(!parseProfile("[WhitePhial]\nRememberAcrossSaves=1\nFullyReenchanted=2\nRefillHours=12\nHotkey=68\n"));
    assert(!parseProfile("[WhitePhial]\nRememberAcrossSaves=\n"));
    assert(!parseProfile("[WhitePhial]\nRememberAcrossSaves=0\nRememberAcrossSaves=1\n"));
    assert(!parseProfile("[WhitePhial]\nRememberAcrossSaves=0 rubbish\n"));
    assert(!parseProfile("[WhitePhial]\nRememberAcrossSaves=0\nRefillHours=12\nHotkey=68.5\n"));
    assert(!parseProfile(std::string(16385, 'x')));
    assert(!parseProfile("[Unrelated]\nRememberAcrossSaves=1\n"));
    const auto commented = parseProfile("\xEF\xBB\xBF[WhitePhial]\r\n RememberAcrossSaves = 0 ; disabled\r\n[Unrelated]\nHello=world\n");
    assert(commented && !commented.profile.enabled);
    auto disabled = parseProfile(*serializeProfile({ false, {} }));
    assert(disabled && !disabled.profile.enabled);

    // Decimal formatting is stable across Windows regional settings.
    const auto previous = std::locale();
    std::locale::global(std::locale(previous, new CommaDecimal));
    assert(serializeProfile(remembered) == encoded);
    assert(parseProfile(*encoded).profile.values == remembered.values);
    std::locale::global(previous);

    const auto root = std::filesystem::temp_directory_path() /
        ("WhitePhialProfileTest-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = root / "SKSE/Plugins/WhitePhialMenu.ini";
    // Fresh install does not enable or create global overrides.
    const auto missing = loadProfile(path);
    assert(missing && !missing.profile.enabled && !std::filesystem::exists(path));
    assert(saveProfile(path, remembered).empty());
    // Read again from disk, with no previous Profile object: survives restart.
    auto restarted = loadProfile(path);
    assert(restarted && restarted.profile.enabled && restarted.profile.values == remembered.values);
    // Replacement updates all three values together and keeps them independent of saves.
    remembered.values = { 0, 0.1f, 181 };
    assert(saveProfile(path, remembered).empty());
    auto replaced = loadProfile(path);
    assert(replaced && replaced.profile.values == remembered.values);
    auto invalid = remembered;
    invalid.values[hours] = std::numeric_limits<float>::infinity();
    assert(!saveProfile(path, invalid).empty());
    assert(loadProfile(path).profile.values == remembered.values);

    // A write failure must not destroy the last valid profile.
    auto temporary = path;
    temporary += ".tmp";
    std::filesystem::create_directory(temporary);
    assert(!saveProfile(path, { true, { 1, 48, 68 } }).empty());
    assert(loadProfile(path).profile.values == remembered.values);
    std::filesystem::remove(temporary);

    // Turning sharing off is itself persistent; it cannot reactivate on restart.
    assert(saveProfile(path, { false, remembered.values }).empty());
    auto off = loadProfile(path);
    assert(off && !off.profile.enabled);
    std::filesystem::remove_all(root);
}
