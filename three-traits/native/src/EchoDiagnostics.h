#pragma once
#include <cstdint>

// Observes the existing combat callbacks. No new event source, timer or script.
// Accessed on the game thread while main.cpp holds stateMutex.
struct EchoDiagnostics {
    bool enabled = false, notifications = false;
    unsigned remaining = 0;
    int selectedState = -1, attackState = -1;
    std::uint64_t swing = 0, notifiedSwing = 0;

    void configure() {
        constexpr auto path = L".\\Data\\SKSE\\Plugins\\BiggieTraitMechanics.ini";
        enabled = GetPrivateProfileIntW(L"EchoingSteelDiagnostics", L"Enabled", 0, path) != 0;
        notifications = GetPrivateProfileIntW(L"EchoingSteelDiagnostics", L"Notifications", 0, path) != 0;
        SKSE::log::info("Echoing Steel diagnostics: enabled={}, notifications={}, limit=200 records per load",
            enabled, notifications);
        reset();
    }
    void reset() {
        remaining = enabled ? 200 : 0;
        selectedState = attackState = -1;
        swing = notifiedSwing = 0;
    }
    bool take() {
        if (!enabled || !remaining) return false;
        if (--remaining == 0)
            SKSE::log::info("[EchoDiag] LIMIT: automatic capture is stopping; reload a save to capture again");
        return true;
    }
    void selected(bool value, RE::FormID id) {
        if (!enabled || selectedState == int(value)) return;
        selectedState = int(value);
        if (take()) {
            SKSE::log::info("[EchoDiag] TRAIT: selected={}, ability={:08X}, combat hooks installed", value, id);
            if (notifications) RE::DebugNotification(value ?
                "Echoing Steel: diagnostic capture started." :
                "Echoing Steel diagnostics: trait is not selected.");
        }
    }
    void armed(const char* source, double until) {
        if (take()) {
            SKSE::log::info("[EchoDiag] ARMED: source={}, expires_at={:.3f}, current_swing={}", source, until, swing);
            if (notifications) RE::DebugNotification("Echoing Steel: primed (5 seconds).");
        }
    }
    void begin(const char* source, bool selected, bool bash, bool power, bool melee, bool twoHanded,
        double now, double until, bool held, float multiplier) {
        ++swing;
        if (take()) SKSE::log::info(
            "[EchoDiag] ATTACK: swing={}, source={}, selected={}, bash={}, power={}, melee={}, two_handed={}, "
            "time={:.3f}, echo_expiry_before={:.3f}, held_before={}, multiplier={:.2f}",
            swing, source, selected, bash, power, melee, twoHanded, now, until, held, multiplier);
    }
    void applied(bool boosted) {
        if (boosted && notifications && swing != notifiedSwing) {
            notifiedSwing = swing;
            // Called only from a captured hit whose damage was actually multiplied.
            // This confirms this plugin's HitData change, not final HP loss after all mods.
            RE::DebugNotification("Echoing Steel: bonus applied to hit.");
        }
    }
};
