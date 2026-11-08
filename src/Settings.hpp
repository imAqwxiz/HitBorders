#pragma once
#include <Geode/Geode.hpp>

// Tiny persisted on/off state for Hitborders.
namespace hb {

inline bool& enabledRef() {
    static bool value = geode::Mod::get()->getSavedValue<bool>("hitborders", true);
    return value;
}

inline bool enabled() { return enabledRef(); }

inline void setEnabled(bool v) {
    enabledRef() = v;
    geode::Mod::get()->setSavedValue<bool>("hitborders", v);
}

}
