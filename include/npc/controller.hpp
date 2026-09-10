#pragma once
#include "npc/world.hpp"
namespace npc {
    // A deliberately bounded experimental policy, not a universal HTN planner.
    // Input is a local projection only: there is no World/State argument or back-reference.
    struct Candidate {
        std::string method;
        double score = 0;
        double expected_minutes = 0;
        std::vector < std::string > assumptions;
        std::optional < Command > first;
    };
    struct Decision {
        Id actor;
        Minute time = 0;
        std::string goal;
        std::optional < Command > command;
        std::vector < Candidate > candidates;
    };
    Decision decide(const Config & config, const Json & view, const Json & objective = Object {});
    Json describe(const Decision & decision);
}
