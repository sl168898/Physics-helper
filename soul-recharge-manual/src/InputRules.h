#pragma once
#include <cstdint>
#include <initializer_list>
namespace sr {
constexpr bool bindable(std::uint32_t key) {
    if(key<2 || key>255) return false;
    // Reserve the console and standalone modifiers. Escape cancels capture.
    for(auto reserved:{29u,42u,54u,56u,41u,157u,184u,219u,220u,221u}) if(key==reserved) return false;
    return true;
}
enum class InputAction { ignore,recharge,snapshot };
constexpr InputAction route(std::uint32_t key,std::uint32_t binding,std::uint32_t snapshot,bool down,bool ready,bool blocked,bool capturing) {
    if(!ready || blocked || capturing || !down) return InputAction::ignore;
    if(key==binding) return InputAction::recharge;
    if(key==snapshot) return InputAction::snapshot;
    return InputAction::ignore;
}
}
