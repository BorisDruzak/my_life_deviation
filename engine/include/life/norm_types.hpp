#pragma once

#include "life/semantic_types.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <optional>

namespace life {

enum class NormMode : std::uint8_t {
    Legacy,
    Shadow,
    Enabled,
    FrozenLearning,
    NoNormDecisionEffects
};

enum class NormChannel : std::uint8_t {
    Descriptive,
    Approval,
    Detection,
    Classification,
    Reaction,
    PersonalPrinciple
};

enum class NormOrigin : std::uint8_t {
    Observed,
    Reported,
    Historical,
    Assumed,
    LegacyPrior
};

enum class ApprovalValue : std::uint8_t {
    Approve,
    Disapprove,
    Indifferent
};

enum class ApplyEvidence : std::uint8_t {
    Added,
    Replaced,
    Duplicate,
    Unknown,
    Rejected
};

struct NormKey {
    std::uint32_t practice{}, local_group{}, context{}, actor_role{}, variant{};
    auto operator<=>(const NormKey&) const = default;
    template<class A> void fields(A& a) { a(practice,local_group,context,actor_role,variant); }
};

struct NormSource {
    std::uint64_t delivery{}, known_root{}, revision{};
    Id speaker{};
    NormOrigin origin{};
    bool independently_grounded{};
    auto operator<=>(const NormSource&) const = default;
    template<class A> void fields(A& a) { a(delivery,known_root,revision,speaker,origin,independently_grounded); }
};

struct NormObservation {
    NormKey key{};
    NormSource source{};
    Tick at{};
    Id observed_actor{};
    double quality{}, confidence{}, reliability{1}, dose{};
    double value{};
    NormChannel channel{};
    ApprovalValue approval{};
    bool applicable{}, value_known{}, outcome_window_complete{};
    bool outcome_severity_known{};
    double outcome_severity{};

    // Personal-principle reflection is explicit and never inferred from prevalence.
    bool accepted_argument{};
    double plasticity{};
    std::uint32_t principle_aspect{};
    double principle_applicability{1}, principle_exception{}, principle_severity{1};

    template<class A> void fields(A& a) {
        a(key,source,at,observed_actor,quality,confidence,reliability,dose,value,
          channel,approval,applicable,value_known,outcome_window_complete,
          outcome_severity_known,outcome_severity,
          accepted_argument,plasticity,principle_aspect,principle_applicability,
          principle_exception,principle_severity);
    }
};

struct BinaryNormEstimate {
    double positive{}, negative{};
    double prior_positive{1}, prior_negative{1};
    bool has_explicit_prior{};

    bool known() const;
    double probability() const;
    double coverage(double kappa=4) const;
    template<class A> void fields(A& a) { a(positive,negative,prior_positive,prior_negative,has_explicit_prior); }
};

struct ApprovalEstimate {
    std::array<double,3> evidence{};
    std::array<double,3> prior{1,1,1};
    bool has_explicit_prior{};

    bool known() const;
    std::array<double,3> probabilities() const;
    double coverage(double kappa=4) const;
    template<class A> void fields(A& a) { a(evidence,prior,has_explicit_prior); }
};

struct NormPrediction {
    NormKey key{};
    bool descriptive_known{}, approval_known{}, sanction_known{};
    double prevalence{}, coverage{}, approve{}, disapprove{}, indifferent{}, approval_coverage{};
    double seen{}, classified{}, reacted{}, severity{};
    bool seen_known{}, classified_known{}, reacted_known{};
    bool severity_known{};
    bool personal_principle_known{};
    double principle_weight{}, personal_applicability{1}, personal_exception{}, personal_severity{1};
    double personal_resistance{};
    NormOrigin descriptive_origin{}, approval_origin{}, sanction_origin{}, personal_origin{};
    std::uint64_t own_revision{};

    std::optional<double> sanction_cost() const;

    template<class A> void fields(A& a) {
        a(key,descriptive_known,approval_known,sanction_known,prevalence,coverage,
          approve,disapprove,indifferent,approval_coverage,seen,classified,reacted,
          severity,seen_known,classified_known,reacted_known,severity_known,
          personal_principle_known,principle_weight,personal_applicability,
          personal_exception,personal_severity,personal_resistance,
          descriptive_origin,approval_origin,sanction_origin,personal_origin,
          own_revision);
    }
};

struct NormDecisionView {
    NormMode mode{NormMode::Legacy};
    std::array<NormPrediction,4> considered{};
    std::uint8_t count{};
    template<class A> void fields(A& a) { a(mode,considered,count); }
};

struct NormProfile {
    NormMode mode{NormMode::Legacy};
    double half_life_days{30}, coverage_kappa{4};
    double personal_learning_factor{.02}, motive_on{.12}, motive_off{.07};
    std::uint32_t inbox_limit{32}, hot_record_limit{128};
    std::uint32_t candidate_ids_limit{32}, deep_norm_limit{4};
    std::uint32_t group_limit{4}, new_option_limit{2};

    void validate() const;
    auto operator<=>(const NormProfile&) const = default;
    template<class A> void fields(A& a) {
        a(mode,half_life_days,coverage_kappa,personal_learning_factor,motive_on,
          motive_off,inbox_limit,hot_record_limit,candidate_ids_limit,
          deep_norm_limit,group_limit,new_option_limit);
    }
};

struct NormContextSnapshot {
    Tick now{};
    std::uint64_t own_revision{};
    std::array<NormKey,4> selected_keys{};
    std::array<double,4> goal_relevance{};
    std::uint8_t count{};
    template<class A> void fields(A& a) { a(now,own_revision,selected_keys,goal_relevance,count); }
};

} // namespace life
