#include "test.hpp"
#include "life/decision_ledger.hpp"

#include <cmath>
#include <limits>
#include <optional>

using namespace life;

namespace {
ConsequenceTerm term(
    ConsequenceKind kind,
    double amount,
    Id target = 0,
    Id object = 0,
    std::uint64_t horizon = 0) {
    return {{kind, target, object, horizon}, ForecastOwner::PhysicalModel, amount};
}

struct LedgerTermsLoader {
    std::vector<ConsequenceTerm> value;
    void operator()(std::vector<ConsequenceTerm>& output) const {
        output = value;
    }
};
} // namespace

TEST("norm_ledger", NM_050_duplicate_consequence_key_is_an_error) {
    DecisionLedger ledger;
    ledger.insert_unique(term(ConsequenceKind::ExternalSanction, -.2, 7, 11, 1));
    auto duplicate = term(ConsequenceKind::ExternalSanction, -.7, 7, 11, 1);
    duplicate.owner = ForecastOwner::NormPrior;
    duplicate.source_revision = 42;
    THROWS(ledger.insert_unique(duplicate));
    CHECK(ledger.terms().size() == 1);
    NEAR(ledger.present_value(.035), -.2, 1e-15);
}

TEST("norm_ledger", norm_source_metadata_does_not_change_consequence_identity) {
    DecisionLedger ledger;
    ConsequenceTerm first{{ConsequenceKind::PersonalPrinciple, 1, 5, 0},
                          ForecastOwner::NormPrior, -.4};
    first.source_revision = 10;
    first.norm_key = {5, 0, 0, 0, 1};
    ledger.insert_unique(first);
    auto duplicate = first;
    duplicate.source_revision = 11;
    duplicate.norm_key.variant = 2;
    bool rejected = false;
    try {
        ledger.insert_unique(duplicate);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);
    CHECK(ledger.terms().front().norm_key == first.norm_key);
}

TEST("norm_ledger", NM_051_raw_terms_are_squashed_once_after_sum) {
    DecisionLedger ledger;
    ledger.insert_unique(term(ConsequenceKind::Enjoyment, .8));
    ledger.insert_unique(term(ConsequenceKind::Time, -.2));
    NEAR(ledger.present_value(.035), .6, 1e-15);
    NEAR(ledger.score(.035), .6 / 1.6, 1e-15);
}

TEST("norm_ledger", NM_052_sanction_uses_the_conditional_probability_chain) {
    const auto cost = expected_sanction_cost(.4, .8, .5, .6);
    CHECK(cost.has_value());
    NEAR(*cost, .096, 1e-15);
}

TEST("norm_ledger", NM_053_unknown_sanction_component_stays_unknown) {
    const auto unknown = expected_sanction_cost(.4, .8, std::nullopt, .6);
    CHECK(!unknown.has_value());
    const auto unknown_severity =
        expected_sanction_cost(.4, .8, .5, std::nullopt);
    CHECK(!unknown_severity.has_value());

    auto assumed = term(ConsequenceKind::ExternalSanction, -.096, 7, 11, 1);
    assumed.owner = ForecastOwner::NormPrior;
    assumed.knownness = ConsequenceKnownness::Assumed;
    THROWS(DecisionLedger{}.insert_unique(assumed));
    assumed.source_revision = 17;
    DecisionLedger ledger;
    ledger.insert_unique(assumed);
    CHECK(ledger.terms().front().knownness == ConsequenceKnownness::Assumed);
    CHECK(ledger.terms().front().source_revision == 17);
}

TEST("norm_ledger", NM_054_present_value_term_is_not_discounted_twice) {
    constexpr double discount = .035;
    constexpr double hours = 5;
    const double already_present = .8 * std::exp(-discount * hours);

    auto continuation = term(ConsequenceKind::MaterialAccess, already_present, 0, 23, 5);
    continuation.owner = ForecastOwner::Procedure;
    continuation.time_hours = hours;
    continuation.present_value = true;
    DecisionLedger ledger;
    ledger.insert_unique(continuation);
    NEAR(ledger.present_value(discount), already_present, 1e-15);
}

TEST("norm_ledger", NM_055_specific_experience_blends_instead_of_adding_a_prior) {
    const double probability = blend_social_outcome_probability(
        .9, 1.0,
        .1, 1.0,
        .2, 1.0);
    NEAR(probability, .9, 1e-15);

    DecisionLedger ledger;
    auto outcome = term(ConsequenceKind::SocialAcceptance, .7, 5, 0, 1);
    outcome.owner = ForecastOwner::DirectExperience;
    outcome.probability = probability;
    ledger.insert_unique(outcome);
    CHECK(ledger.terms().size() == 1);
    NEAR(ledger.present_value(0), .63, 1e-15);
}

TEST("norm_ledger", NM_056_self_prior_is_one_blended_forecast_not_a_second_cost) {
    const double probability = blend_social_outcome_probability(
        .8, .75,
        .4, .5,
        .2, .6);
    // p_context=.7, w_self=.01875, p_final=.690625.
    NEAR(probability, .690625, 1e-15);

    DecisionLedger ledger;
    auto outcome = term(ConsequenceKind::SocialAcceptance, -1, 5, 0, 1);
    outcome.owner = ForecastOwner::DirectExperience;
    outcome.probability = probability;
    ledger.insert_unique(outcome);
    CHECK(ledger.terms().size() == 1);
    NEAR(ledger.present_value(0), -.690625, 1e-15);
}

TEST("norm_ledger", NM_057_one_person_in_two_groups_has_one_reaction_key) {
    DecisionLedger ledger;
    auto colleague = term(ConsequenceKind::SocialAcceptance, .3, 9, 44, 2);
    colleague.owner = ForecastOwner::NormPrior;
    colleague.source_revision = 100;
    ledger.insert_unique(colleague);

    auto friend_role = colleague;
    friend_role.amount = .6;
    friend_role.source_revision = 101;
    THROWS(ledger.insert_unique(friend_role));
    CHECK(ledger.terms().size() == 1);
}

TEST("norm_ledger", canonical_order_does_not_depend_on_insertion_order) {
    const auto a = term(ConsequenceKind::Time, -.2, 2, 3, 4);
    const auto b = term(ConsequenceKind::Enjoyment, .8, 4, 3, 2);
    DecisionLedger forward;
    forward.insert_unique(a);
    forward.insert_unique(b);
    DecisionLedger reverse;
    reverse.insert_unique(b);
    reverse.insert_unique(a);
    CHECK(forward.terms().size() == reverse.terms().size());
    for (std::size_t i = 0; i < forward.terms().size(); ++i) {
        CHECK(forward.terms()[i].key == reverse.terms()[i].key);
        CHECK(forward.terms()[i].amount == reverse.terms()[i].amount);
    }
}

TEST("norm_ledger", invalid_or_unjustified_numbers_are_rejected) {
    auto invalid = term(ConsequenceKind::Enjoyment, std::numeric_limits<double>::quiet_NaN());
    THROWS(DecisionLedger{}.insert_unique(invalid));
    invalid = term(ConsequenceKind::Enjoyment, .2);
    invalid.probability = 1.01;
    THROWS(DecisionLedger{}.insert_unique(invalid));
    invalid.probability = 1;
    invalid.time_hours = -1;
    THROWS(DecisionLedger{}.insert_unique(invalid));
    invalid.time_hours = 0;
    invalid.knownness = ConsequenceKnownness::Unknown;
    THROWS(DecisionLedger{}.insert_unique(invalid));

    DecisionLedger ledger;
    ledger.insert_unique(term(ConsequenceKind::Enjoyment, .2));
    THROWS(ledger.present_value(-.01));
    THROWS(ledger.score(std::numeric_limits<double>::infinity()));
}

TEST("norm_ledger", deserialized_terms_cannot_bypass_unique_key_validation) {
    const auto duplicate = term(ConsequenceKind::Enjoyment, .2, 3, 4, 5);
    DecisionLedger ledger;
    LedgerTermsLoader loader{{duplicate, duplicate}};
    ledger.fields(loader);
    THROWS(ledger.present_value(0));
}

TEST("norm_ledger", personal_principle_uses_one_maximum_per_moral_aspect) {
    const MoralAspectTerm aspects[] = {
        {4, 1.0, 1.0, .7, 0},
        {4, 1.0, 1.0, .6, 0},
        {8, .5, .8, .5, .25},
    };
    // max(.7,.6) + .5*.8*.5*(1-.25) = .85.
    NEAR(personal_principle_cost(aspects), .85, 1e-15);
}

// NM-058 requires Planner comparison/intent hysteresis and NM-059 requires
// attention-to-runtime side-effect boundaries. They are integration tests owned
// by the root task; a pure ledger test cannot prove either requirement.
