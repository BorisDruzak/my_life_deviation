#pragma once
#include "npc/config.hpp"
namespace npc {
    struct MinuteEffects {
        std::string mode = "ordinary";
        std::map < std::string, double > needs;
        std::map < std::string, double > exposure;
        std::map < std::string, double > pleasure_rate;
        std::string practiced_skill;
        double task_fit = 1;
    };
    // Same continuous equations are used by the authoritative tick and a local forecast.
    void apply_minute(const Config & c, const std::string & profile, Actor & next, const Actor & old, const MinuteEffects & effects);
}
