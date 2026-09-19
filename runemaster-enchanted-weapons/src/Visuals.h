#pragma once
#include <RE/Skyrim.h>

namespace runes::visuals {
struct Appearance {
    RE::BGSArtObject* art{};
    RE::TESEffectShader* shader{};
    bool operator==(const Appearance&) const = default;
    explicit operator bool() const { return art || shader; }
};
using SelectAppearance = Appearance (*)(RE::Actor*, RE::TESObjectWEAP*);
void install(bool (*enabled)(), SelectAppearance select);
void reset();
}
