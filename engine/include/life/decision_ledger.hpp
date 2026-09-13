#pragma once

#include "life/norm_types.hpp"
#include "life/semantic_types.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace life {

enum class ConsequenceKind : std::uint8_t {
    BodilyRelief,
    Enjoyment,
    MaterialAccess,
    SocialAcceptance,
    ExternalSanction,
    PersonalPrinciple,
    Time,
    Resource,
    Switching,
    ResidualUncertainty
};

enum class ForecastOwner : std::uint8_t {
    DirectExperience,
    NormPrior,
    SelfPrior,
    Procedure,
    PhysicalModel
};

enum class ConsequenceKnownness : std::uint8_t {
    Known,
    Assumed,
    Unknown
};

struct ConsequenceKey {
    ConsequenceKind kind{};
    Id target{};
    Id object{};
    std::uint64_t horizon{};

    auto operator<=>(const ConsequenceKey&) const = default;

    template<class A>
    void fields(A& a) {
        a(kind, target, object, horizon);
    }
};

struct ConsequenceTerm {
    ConsequenceKey key{};
    ForecastOwner owner{};
    double amount{};
    double probability{1};
    double time_hours{};
    bool present_value{};
    std::uint64_t source_revision{};
    ConsequenceKnownness knownness{ConsequenceKnownness::Known};
    NormKey norm_key{};

    template<class A>
    void fields(A& a) {
        a(key, owner, amount, probability, time_hours, present_value,
          source_revision, knownness, norm_key);
    }
};

class DecisionLedger {
public:
    void insert_unique(const ConsequenceTerm& term);
    double present_value(double discount_per_hour) const;
    double score(double discount_per_hour) const;

    const std::vector<ConsequenceTerm>& terms() const noexcept {
        return terms_;
    }

    template<class A>
    void fields(A& a) {
        a(terms_);
    }

private:
    std::vector<ConsequenceTerm> terms_;
};

std::optional<double> expected_sanction_cost(
    std::optional<double> p_seen,
    std::optional<double> p_classified_given_seen,
    std::optional<double> p_reaction_given_classified,
    std::optional<double> subjective_severity);

double blend_social_outcome_probability(
    double p_specific,
    double c_specific,
    double p_norm_or_prior,
    double c_norm,
    double p_self_acceptance,
    double c_self_acceptance);

struct MoralAspectTerm {
    std::uint32_t aspect{};
    double weight{};
    double applicability_confidence{};
    double severity{};
    double exception_probability{};

    template<class A>
    void fields(A& a) {
        a(aspect, weight, applicability_confidence, severity,
          exception_probability);
    }
};

double personal_principle_cost(std::span<const MoralAspectTerm> terms);

} // namespace life
