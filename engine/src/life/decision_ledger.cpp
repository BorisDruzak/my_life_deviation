#include "life/decision_ledger.hpp"

#include "life/numeric.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace life {
namespace {

void require_finite(double value, const char* message) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(message);
    }
}

void validate_term(const ConsequenceTerm& term) {
    if (static_cast<unsigned>(term.key.kind) >
        static_cast<unsigned>(ConsequenceKind::ResidualUncertainty)) {
        throw std::invalid_argument("invalid consequence kind");
    }
    if (static_cast<unsigned>(term.owner) >
        static_cast<unsigned>(ForecastOwner::PhysicalModel)) {
        throw std::invalid_argument("invalid forecast owner");
    }
    if (static_cast<unsigned>(term.knownness) >
        static_cast<unsigned>(ConsequenceKnownness::Unknown)) {
        throw std::invalid_argument("invalid consequence knownness");
    }
    require_finite(term.amount, "non-finite consequence amount");
    require_range(term.probability, 0, 1);
    require_finite(term.time_hours, "non-finite consequence time");
    if (term.time_hours < 0) {
        throw std::invalid_argument("negative consequence time");
    }
    if (term.knownness == ConsequenceKnownness::Unknown) {
        throw std::invalid_argument("unknown consequence has no numeric term");
    }
    if (term.knownness == ConsequenceKnownness::Assumed &&
        term.source_revision == 0) {
        throw std::invalid_argument("assumed consequence needs a source revision");
    }
}

void validate_optional_probability(const std::optional<double>& value) {
    if (value) {
        require_range(*value, 0, 1);
    }
}

} // namespace

void DecisionLedger::insert_unique(const ConsequenceTerm& term) {
    validate_term(term);
    const auto at = std::lower_bound(
        terms_.begin(), terms_.end(), term.key,
        [](const ConsequenceTerm& existing, const ConsequenceKey& key) {
            return existing.key < key;
        });
    if (at != terms_.end() && at->key == term.key) {
        throw std::invalid_argument("duplicate consequence key");
    }
    terms_.insert(at, term);
}

double DecisionLedger::present_value(double discount_per_hour) const {
    require_finite(discount_per_hour, "non-finite discount rate");
    if (discount_per_hour < 0) {
        throw std::invalid_argument("negative discount rate");
    }

    double result = 0;
    const ConsequenceKey* previous = nullptr;
    for (const auto& term : terms_) {
        validate_term(term);
        if (previous && !(*previous < term.key)) {
            throw std::invalid_argument("non-canonical consequence ledger");
        }
        previous = &term.key;
        double value = term.amount * term.probability;
        require_finite(value, "non-finite expected consequence");
        if (!term.present_value) {
            const double exponent = discount_per_hour * term.time_hours;
            require_finite(exponent, "consequence discount interval overflow");
            value *= std::exp(-exponent);
            require_finite(value, "non-finite discounted consequence");
        }
        result += value;
        require_finite(result, "consequence ledger sum overflow");
    }
    return result;
}

double DecisionLedger::score(double discount_per_hour) const {
    const double raw = present_value(discount_per_hour);
    return raw / (1 + std::abs(raw));
}

std::optional<double> expected_sanction_cost(
    std::optional<double> p_seen,
    std::optional<double> p_classified_given_seen,
    std::optional<double> p_reaction_given_classified,
    std::optional<double> subjective_severity) {
    validate_optional_probability(p_seen);
    validate_optional_probability(p_classified_given_seen);
    validate_optional_probability(p_reaction_given_classified);
    validate_optional_probability(subjective_severity);
    if (!p_seen || !p_classified_given_seen ||
        !p_reaction_given_classified || !subjective_severity) {
        return std::nullopt;
    }
    return *p_seen * *p_classified_given_seen *
           *p_reaction_given_classified * *subjective_severity;
}

double blend_social_outcome_probability(
    double p_specific,
    double c_specific,
    double p_norm_or_prior,
    double c_norm,
    double p_self_acceptance,
    double c_self_acceptance) {
    for (double value : {p_specific, c_specific, p_norm_or_prior, c_norm,
                         p_self_acceptance, c_self_acceptance}) {
        require_range(value, 0, 1);
    }
    const double p_context =
        c_specific * p_specific + (1 - c_specific) * p_norm_or_prior;
    const double w_self =
        .25 * (1 - c_specific) * (1 - c_norm) * c_self_acceptance;
    return (1 - w_self) * p_context + w_self * p_self_acceptance;
}

double personal_principle_cost(std::span<const MoralAspectTerm> terms) {
    std::map<std::uint32_t, double> maxima;
    for (const auto& term : terms) {
        for (double value : {term.weight, term.applicability_confidence,
                             term.severity, term.exception_probability}) {
            require_range(value, 0, 1);
        }
        const double resistance =
            term.weight * term.applicability_confidence * term.severity *
            (1 - term.exception_probability);
        maxima[term.aspect] = std::max(maxima[term.aspect], resistance);
    }

    double result = 0;
    for (const auto& [aspect, value] : maxima) {
        (void)aspect;
        result += value;
    }
    return unit(result);
}

} // namespace life
