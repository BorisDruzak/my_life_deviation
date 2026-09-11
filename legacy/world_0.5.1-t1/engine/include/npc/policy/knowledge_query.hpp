#pragma once
#include "npc/policy/types.hpp"

namespace npc::policy {
enum class Truth { KnownTrue, KnownFalse, Unknown };
std::string to_string(Truth truth);

struct BeliefQuery {
    Id subject;
    std::string predicate;
    Json arguments = Object{};
    Minute at = 0;
    bool polarity = true;
};
struct PersistenceRule {
    std::string predicate;
    double half_life_minutes = 60;
    double prior = .5;
};
struct EvidenceUse {
    Id id;
    Id source;
    std::vector<Id> roots;
    double confidence = .5;
    bool assumed = false;
    Minute observed_at = 0;
    Minute applies_at = 0;
};
struct BeliefResult {
    Truth truth = Truth::Unknown;
    double confidence = .5;
    std::string reason = "no_evidence";
    std::vector<EvidenceUse> sources;
    std::size_t examined = 0;
    bool complete = true;
};
BeliefResult query_belief(const LocalView& view, const BeliefQuery& query,
                         const std::optional<PersistenceRule>& persistence = {},
                         std::size_t evidence_limit = local_page_limit);
Json describe(const BeliefResult& result);
} // namespace npc::policy
