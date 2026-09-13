#include "test.hpp"
#include "life/mind.hpp"
#include "valuation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace life;

namespace {

NormPayload payload_for(NormPractice practice, bool question = false,
                        std::uint32_t variant = 0) {
    NormPayload payload;
    payload.present = true;
    payload.question = question;
    payload.key = {practice_id(practice), 1, 1, 0, variant};
    payload.evidence.key = payload.key;
    payload.evidence.source = {10, 10, 1, 2, NormOrigin::Observed, true};
    payload.evidence.channel = NormChannel::Approval;
    payload.evidence.approval = ApprovalValue::Approve;
    payload.evidence.value_known = true;
    payload.evidence.value = 1;
    return payload;
}

PersonalView planner_view() {
    PersonalView view;
    view.self = 1;
    view.place = 7;
    view.now = 10 * 3600000;
    view.capability.gate = 1;
    view.capability.context = 12;
    view.capability.operations = 64;
    view.capability.alternatives = 8;
    view.known.fill(true);
    KnownPlace place;
    place.id = 7;
    place.services = 0xffffffffu;
    place.food = Truth::Confirmed;
    view.places.push_back(place);
    view.norms_view.mode = NormMode::Enabled;
    view.self_enabled = true;
    view.civil.enabled = true;
    view.civil.memory.enabled = true;
    view.civil.profile.budget_policy = BudgetPolicy::Deliberative;
    view.social.enabled = true;
    view.social.self = view.self;
    view.social.place = view.place;
    view.social.now = view.now;
    view.social.norms_view = view.norms_view;
    for (auto& method : view.social.memory.methods) {
        method = {.9, 0, .8, .8, 100};
    }
    return view;
}

void add_shop(PersonalView& view) {
    view.civil.memory.shops.push_back(
        {7, 99, 0, 0, {{10, 35, 0}, {11, 95, 1}, {12, 180, 2}}});
    auto& question =
        view.civil.memory.questions.ensure(QuestionKind::Clothing, 7, 99, 0);
    question.pending = false;
    question.actionable = true;
}

struct ClothingOrderNorm {
    NormPractice practice{NormPractice::ClothingTier};
    bool seen_known{true};
    double seen{1};
    bool audience_known{};
    double applicability{1};
    double exception{};
    double approval_value{1};
    double disapproval_value{1};
    bool sanction_known{};
};

std::vector<Id> clothing_options(double approve, double disapprove,
                                 NormMode mode,
                                 ClothingOrderNorm norm = {}) {
    auto view = planner_view();
    view.known.fill(false);
    view.known[std::size_t(Method::BuyClothes)] = true;
    view.money = 500;
    view.civil.memory.desired_tier = 2;
    add_shop(view);
    view.norms_view.mode = mode;
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = {practice_id(norm.practice), 1, 1, 0,
                      norm.practice == NormPractice::ClothingTier ? 2u : 0u};
    prediction.approval_known = true;
    prediction.approval_coverage = 1;
    prediction.approve = approve;
    prediction.disapprove = disapprove;
    prediction.indifferent = 1 - approve - disapprove;
    prediction.seen_known = norm.seen_known;
    prediction.seen = norm.seen;
    prediction.own_revision = 17;
    if (norm.sanction_known) {
        prediction.sanction_known = true;
        prediction.classified_known = true;
        prediction.reacted_known = true;
        prediction.severity_known = true;
        prediction.classified = 1;
        prediction.reacted = 1;
        prediction.severity = 1;
    }
    auto& effect = view.norm_context.effects[0];
    effect.group_significance = 1;
    effect.approval_value = norm.approval_value;
    effect.disapproval_value = norm.disapproval_value;
    effect.applicability = norm.applicability;
    effect.exception = norm.exception;
    effect.audience_known = norm.audience_known;
    Seed random;
    const auto options = Planner::ideas(view, random);
    std::vector<Id> result;
    for (const auto& option : options) {
        if (option.method == Method::BuyClothes) result.push_back(option.object);
    }
    return result;
}

std::size_t count_method(const std::vector<PlanOption>& options, Method method) {
    return std::count_if(options.begin(), options.end(), [&](const auto& option) {
        return option.method == method;
    });
}

const ConsequenceTerm* find_term(const Decision& decision,
                                 ConsequenceKind kind,
                                 std::uint64_t horizon) {
    const auto at = std::find_if(
        decision.norm_ledger.begin(), decision.norm_ledger.end(),
        [&](const auto& term) {
            return term.key.kind == kind && term.key.horizon == horizon;
        });
    return at == decision.norm_ledger.end() ? nullptr : &*at;
}

} // namespace

TEST("norm_planner", NM_034_low_motive_does_not_force_disapproval) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    auto payload = payload_for(NormPractice::Honesty);
    payload.evidence.approval = ApprovalValue::Disapprove;
    payload.evidence.value = -1;
    view.norm_options = {payload};
    view.norms_view.count = 1;
    view.norms_view.considered[0].key = payload.key;
    view.norm_context.effects[0].group_significance = 0;
    const auto choices = social_choices(view);
    CHECK(std::none_of(choices.begin(), choices.end(), [](const auto& choice) {
        return choice.kind == Interaction::DisapprovePractice;
    }));
}

TEST("norm_planner", NM_035_active_group_goal_can_create_approval_choice) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    auto payload = payload_for(NormPractice::Honesty);
    view.norm_options = {payload};
    view.norms_view.count = 1;
    view.norms_view.considered[0].key = payload.key;
    view.norm_context.effects[0].group_significance = .8;
    const auto choices = social_choices(view);
    const auto found = std::find_if(choices.begin(), choices.end(), [](const auto& choice) {
        return choice.kind == Interaction::ApprovePractice;
    });
    CHECK(found != choices.end());
    CHECK(found->norm_payload.key == payload.key);
}

TEST("norm_planner", personal_principle_can_motivate_targeted_disapproval) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    auto payload = payload_for(NormPractice::Honesty);
    payload.subject = 2;
    payload.evidence.approval = ApprovalValue::Disapprove;
    payload.evidence.value = -1;
    view.norm_options = {payload};
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = payload.key;
    prediction.personal_principle_known = true;
    prediction.personal_resistance = .8;
    const auto choices = social_choices(view);
    CHECK(std::any_of(choices.begin(), choices.end(), [](const auto& choice) {
        return choice.kind == Interaction::DisapprovePractice && choice.other == 2;
    }));
}

TEST("norm_planner", NM_055_specific_approval_is_one_contextual_norm_outcome) {
    auto view = planner_view();
    view.social.in_conversation = true;
    view.social.partner = 2;
    auto payload = payload_for(NormPractice::Honesty);
    view.social.norm_payload = payload;
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = payload.key;
    prediction.approval_known = true;
    prediction.sanction_known = true;
    prediction.approve = .2;
    prediction.disapprove = .7;
    prediction.seen = .5;
    prediction.classified = .8;
    prediction.reacted = .5;
    prediction.severity = .6;
    prediction.coverage = 1;
    prediction.own_revision = 17;
    view.norm_context.effects[0] = {5, 2, .8, .6, .4, 1, 0, true};
    view.social.norms_view = view.norms_view;
    view.social.norm_context = view.norm_context;
    view.social.memory.experiences.push_back(
        {Interaction::ApprovePractice, 2, 0, false, {}, {}, 1, 1, 4, 0, 0, 0, 0, 0});
    view.social.memory.experiences[0].approval.mean = .9;
    view.social.memory.experiences[0].approval.count = 20;
    view.social.memory.experiences[0].last_event = 88;

    PlanOption option;
    option.method = Method::Social;
    option.place = 7;
    option.partner = 2;
    option.interaction = Interaction::ApprovePractice;
    option.norm_payload = payload;
    const auto decision = Planner::forecast(view, option);
    CHECK(decision.path.size() == 1);
    CHECK(std::count_if(decision.norm_ledger.begin(), decision.norm_ledger.end(),
                        [](const auto& term) {
                            return term.key.kind == ConsequenceKind::SocialAcceptance;
                        }) == 1);
    const auto* acceptance = find_term(decision, ConsequenceKind::SocialAcceptance, 1);
    CHECK(acceptance != nullptr);
    CHECK(acceptance->owner == ForecastOwner::DirectExperience);
    CHECK(acceptance->source_revision == 88);
}

TEST("norm_planner", partial_seen_knowledge_keeps_approval_but_not_unknown_sanction) {
    auto view = planner_view();
    auto payload = payload_for(NormPractice::Work);
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = payload.key;
    prediction.approval_known = true;
    prediction.approval_coverage = 1;
    prediction.approve = .8;
    prediction.disapprove = .1;
    prediction.indifferent = .1;
    prediction.seen_known = true;
    prediction.seen = .5;
    prediction.own_revision = 17;
    view.norm_context.effects[0] = {5, 2, 1, 1, 1, 1, 0, false};
    PlanOption option{Method::Rest, 7, 0, 0};
    option.norm_payload = payload;
    const auto decision = Planner::forecast(view, option);
    CHECK(find_term(decision, ConsequenceKind::SocialAcceptance, 1) != nullptr);
    CHECK(find_term(decision, ConsequenceKind::ExternalSanction, 1) == nullptr);

    view.norm_context.effects[0].applicability = 0;
    const auto inapplicable = Planner::forecast(view, option);
    CHECK(find_term(inapplicable, ConsequenceKind::SocialAcceptance, 1) == nullptr);
    CHECK(find_term(inapplicable, ConsequenceKind::ExternalSanction, 1) == nullptr);
    view.norm_context.effects[0].applicability = 1;
    view.norm_context.effects[0].exception = 1;
    const auto excepted = Planner::forecast(view, option);
    CHECK(find_term(excepted, ConsequenceKind::SocialAcceptance, 1) == nullptr);
    CHECK(find_term(excepted, ConsequenceKind::ExternalSanction, 1) == nullptr);
}

TEST("norm_planner", NM_056_self_cost_is_residual_after_detailed_social_forecast) {
    auto view = planner_view();
    view.social.in_conversation = true;
    view.social.partner = 2;
    auto payload = payload_for(NormPractice::Honesty);
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = payload.key;
    prediction.approval_known = true;
    prediction.approve = .8;
    prediction.disapprove = .1;
    prediction.indifferent = .1;
    prediction.seen = 1;
    prediction.coverage = 1;
    prediction.own_revision = 17;
    view.norm_context.effects[0] = {5, 2, 1, 1, 1, 1, 0, true};
    view.social.norms_view = view.norms_view;
    view.social.norm_context = view.norm_context;
    for (auto& self : view.self_predictions) {
        self.acceptance = .1;
        self.axis_confidence[std::size_t(SelfAxis::Acceptance)] = 1;
    }

    PlanOption option;
    option.method = Method::Social;
    option.place = 7;
    option.partner = 2;
    option.interaction = Interaction::ApprovePractice;
    option.norm_payload = payload;
    const auto decision = Planner::forecast(view, option);
    CHECK(std::count_if(decision.norm_ledger.begin(), decision.norm_ledger.end(),
                        [](const auto& term) {
                            return term.key.kind == ConsequenceKind::SocialAcceptance;
                        }) == 1);
    CHECK(std::count_if(decision.norm_ledger.begin(), decision.norm_ledger.end(),
                        [](const auto& term) {
                            return term.owner == ForecastOwner::SelfPrior;
                        }) <= 1);
}

TEST("norm_planner", return_promise_has_explicit_owner_without_changing_score) {
    auto view = planner_view();
    view.self_enabled = false;
    view.need.fill(0);
    view.social.memory.rejection_sensitivity = 0;
    view.norms_view.count = 1;
    auto& promise = view.norms_view.considered[0];
    promise.key = {practice_id(NormPractice::Promise), 0, 0, 0, 0};
    promise.personal_principle_known = true;
    promise.personal_resistance = .8;
    promise.own_revision = 17;
    ItemMemory item;
    item.id = 10;
    item.owner = 2;
    item.holder = 1;
    item.owner_status = Truth::Confirmed;
    item.holder_status = Truth::Confirmed;
    item.uses = 1;
    view.social.memory.items.push_back(item);

    PlanOption option;
    option.method = Method::Social;
    option.place = view.place;
    option.partner = 2;
    option.interaction = Interaction::ReturnItem;
    option.object = item.id;
    const auto decision = Planner::forecast(view, option);
    CHECK(decision.path.size() == 1);
    CHECK(decision.social_evaluation.personal_instrumental > 0);
    CHECK(decision.social_evaluation.personal_instrumental_key == promise.key);
    CHECK(decision.social_evaluation.personal_instrumental_revision ==
          promise.own_revision);

    const auto* principle =
        find_term(decision, ConsequenceKind::PersonalPrinciple, 0);
    CHECK(principle != nullptr);
    CHECK(principle->owner == ForecastOwner::NormPrior);
    CHECK(principle->source_revision == promise.own_revision);
    CHECK(principle->amount > 0);

    const double hours =
        double(interaction_duration(Interaction::ReturnItem)) / 3600000.;
    const double old_expected = .8 * .65 * (.25 + .40 * .8);
    const double old_raw =
        old_expected * std::exp(-cog04::cfg::discount_per_hour * hours) -
        (1 - .8) * .03 - hours / 24 * .10;
    NEAR(decision.norm_raw, old_raw, 1e-15);
    NEAR(decision.score, old_raw / (1 + std::abs(old_raw)), 1e-15);
}

TEST("norm_planner", receiving_work_promise_exposes_the_paid_owner) {
    auto view = planner_view().social;
    view.life_enabled = true;
    view.employed = true;
    view.supervisor = 2;
    view.norms_view.count = 1;
    auto& promise = view.norms_view.considered[0];
    promise.key = {practice_id(NormPractice::Promise), 0, 0, 0, 0};
    promise.personal_principle_known = true;
    promise.personal_resistance = .8;
    promise.own_revision = 19;

    const auto receiving =
        evaluate_social(view, Interaction::RequestWork, 2, 0, true);
    NEAR(receiving.instrumental, .25 + .4 * .8, 1e-15);
    NEAR(receiving.personal_instrumental, .4 * .8, 1e-15);
    CHECK(receiving.personal_instrumental_key == promise.key);
    CHECK(receiving.personal_instrumental_revision == promise.own_revision);

    const auto initiating =
        evaluate_social(view, Interaction::RequestWork, 2, 0, false);
    NEAR(initiating.instrumental, .35, 1e-15);
    NEAR(initiating.personal_instrumental, 0, 1e-15);
    CHECK(initiating.personal_instrumental_revision == 0);
}

TEST("norm_planner", NM_060_enabled_clothing_keeps_three_known_skus) {
    auto view = planner_view();
    view.money = 500;
    view.civil.memory.desired_tier = 2;
    add_shop(view);
    const auto options = civil_options(view);
    CHECK(count_method(options, Method::BuyClothes) == 3);
}

TEST("norm_planner", paid_clothing_approval_breaks_only_equal_salience_ties) {
    const auto approved = clothing_options(.8, .1, NormMode::Enabled);
    CHECK(approved.size() == 3);
    CHECK(approved == std::vector<Id>({12, 10, 11}));

    const auto disapproved = clothing_options(.1, .8, NormMode::Enabled);
    CHECK(disapproved.size() == 3);
    CHECK(disapproved == std::vector<Id>({10, 11, 12}));

    const auto no_effects =
        clothing_options(.8, .1, NormMode::NoNormDecisionEffects);
    CHECK(no_effects.size() == 3);
    CHECK(no_effects == std::vector<Id>({10, 11, 12}));

    ClothingOrderNorm condition_norm;
    condition_norm.practice = NormPractice::ClothingCondition;
    const auto condition =
        clothing_options(.8, .1, NormMode::Enabled, condition_norm);
    CHECK(condition.size() == 3);
    CHECK(condition == std::vector<Id>({10, 11, 12}));
}

TEST("norm_planner", clothing_tier_order_ignores_unknown_detection) {
    ClothingOrderNorm norm;
    norm.seen_known = false;
    const auto options = clothing_options(.8, .1, NormMode::Enabled, norm);
    CHECK(options == std::vector<Id>({10, 11, 12}));
}

TEST("norm_planner", clothing_tier_order_ignores_known_zero_detection) {
    ClothingOrderNorm norm;
    norm.seen = 0;
    const auto options = clothing_options(.8, .1, NormMode::Enabled, norm);
    CHECK(options == std::vector<Id>({10, 11, 12}));
}

TEST("norm_planner", clothing_tier_order_accepts_a_known_visible_audience) {
    ClothingOrderNorm norm;
    norm.seen_known = false;
    norm.seen = 0;
    norm.audience_known = true;
    const auto options = clothing_options(.8, .1, NormMode::Enabled, norm);
    CHECK(options == std::vector<Id>({12, 10, 11}));
}

TEST("norm_planner", clothing_tier_order_requires_applicable_norm) {
    ClothingOrderNorm inapplicable;
    inapplicable.applicability = 0;
    const auto ignored =
        clothing_options(.8, .1, NormMode::Enabled, inapplicable);
    CHECK(ignored == std::vector<Id>({10, 11, 12}));
}

TEST("norm_planner", clothing_tier_order_honors_an_applicable_exception) {
    ClothingOrderNorm excepted;
    excepted.exception = 1;
    const auto excluded = clothing_options(.8, .1, NormMode::Enabled, excepted);
    CHECK(excluded == std::vector<Id>({10, 11, 12}));
}

TEST("norm_planner", clothing_tier_order_requires_valued_social_outcome) {
    ClothingOrderNorm norm;
    norm.approval_value = 0;
    norm.disapproval_value = 0;
    const auto options = clothing_options(.8, .1, NormMode::Enabled, norm);
    CHECK(options == std::vector<Id>({10, 11, 12}));
}

TEST("norm_planner", clothing_tier_order_includes_expected_sanction_cost) {
    ClothingOrderNorm norm;
    norm.sanction_known = true;
    const auto options = clothing_options(.8, .1, NormMode::Enabled, norm);
    CHECK(options == std::vector<Id>({10, 11, 12}));
}

TEST("norm_planner", NM_061_clothing_condition_is_not_a_prestige_reward) {
    auto view = planner_view();
    view.money = 500;
    view.civil.memory.garment_condition = .9;
    view.civil.memory.garment_tier = 0;
    add_shop(view);
    PlanOption basic{Method::BuyClothes, 7, 0, 0};
    basic.object = 10;
    PlanOption expensive{Method::BuyClothes, 7, 0, 0};
    expensive.object = 12;
    NEAR(civil_expected_gain(view, basic), 0, 1e-15);
    NEAR(civil_expected_gain(view, expensive), 0, 1e-15);
}

TEST("norm_planner", enabled_mode_never_reads_legacy_normative_arrays) {
    auto view = planner_view();
    const auto method = std::size_t(Method::Rest);
    view.norms[method] = std::numeric_limits<double>::quiet_NaN();
    view.risks[method] = std::numeric_limits<double>::quiet_NaN();
    PlanOption option{Method::Rest, 7, 0, 0};
    const auto decision = Planner::forecast(view, option);
    CHECK(decision.path.size() == 1);
    CHECK(std::isfinite(decision.score));
    NEAR(decision.moral, 0, 1e-15);
    NEAR(decision.risk, 0, 1e-15);
}

TEST("norm_planner", no_effects_mode_uses_nonnormative_ledger_only) {
    auto view = planner_view();
    view.norms_view.mode = NormMode::NoNormDecisionEffects;
    view.social.norms_view.mode = NormMode::NoNormDecisionEffects;
    const auto method = std::size_t(Method::Rest);
    view.norms[method] = std::numeric_limits<double>::quiet_NaN();
    view.risks[method] = std::numeric_limits<double>::quiet_NaN();
    const auto payload = payload_for(NormPractice::Work);
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = payload.key;
    prediction.approval_known = true;
    prediction.approval_coverage = 1;
    prediction.approve = 1;
    prediction.own_revision = 17;
    view.norm_context.effects[0] = {5, 2, 1, 1, 1, 1, 0, true};
    PlanOption option{Method::Rest, 7, 0, 0};
    option.norm_payload = payload;
    const auto decision = Planner::forecast(view, option);
    CHECK(decision.path.size() == 1);
    CHECK(std::isfinite(decision.score));
    CHECK(std::none_of(decision.norm_ledger.begin(), decision.norm_ledger.end(),
                       [](const auto& term) {
                           return term.key.kind == ConsequenceKind::SocialAcceptance ||
                                  term.key.kind == ConsequenceKind::ExternalSanction ||
                                  term.key.kind == ConsequenceKind::PersonalPrinciple;
                       }));
}

TEST("norm_planner", no_effects_mode_keeps_expanded_clothing_strategy) {
    auto view = planner_view();
    view.norms_view.mode = NormMode::NoNormDecisionEffects;
    view.civil.memory.desired_tier = 2;
    view.money = 500;
    add_shop(view);
    CHECK(count_method(civil_options(view), Method::BuyClothes) == 3);
}

TEST("norm_planner", no_effects_mode_has_no_norm_interaction_motive) {
    auto view = planner_view().social;
    view.norms_view.mode = NormMode::NoNormDecisionEffects;
    view.in_conversation = true;
    view.partner = 2;
    const auto payload = payload_for(NormPractice::Honesty);
    view.norm_options = {payload};
    view.norms_view.count = 1;
    view.norms_view.considered[0].key = payload.key;
    view.norm_context.effects[0].group_significance = 1;
    const auto choices = social_choices(view);
    CHECK(std::none_of(choices.begin(), choices.end(), [](const auto& choice) {
                           return choice.kind == Interaction::ApprovePractice;
                       }));
}

TEST("norm_planner", NM_063_guarded_budget_keeps_the_reserve_filter) {
    auto view = planner_view();
    view.money = 220;
    view.civil.budget.protected_cash = 200;
    view.civil.profile.budget_policy = BudgetPolicy::GuardedBaseline;
    add_shop(view);
    const auto options = civil_options(view);
    CHECK(count_method(options, Method::BuyClothes) == 0);
}

TEST("norm_planner", NM_064_deliberative_budget_prices_the_marginal_buffer) {
    auto view = planner_view();
    view.money = 220;
    view.civil.budget.protected_cash = 200;
    view.civil.profile.budget_policy = BudgetPolicy::Deliberative;
    view.civil.profile.risk_importance = .8;
    add_shop(view);
    const auto options = civil_options(view);
    const auto at = std::find_if(options.begin(), options.end(), [](const auto& option) {
        return option.method == Method::BuyClothes && option.object == 11;
    });
    CHECK(at != options.end());
    const auto decision = Planner::forecast(view, *at);
    const auto* buffer = find_term(decision, ConsequenceKind::Resource, 1);
    CHECK(buffer != nullptr);
    NEAR(buffer->amount, -.3, 1e-15);
}

TEST("norm_planner", NM_070_norm_question_allows_spontaneous_known_contact) {
    auto view = planner_view();
    view.known.fill(false);
    view.known[std::size_t(Method::Talk)] = true;
    view.social.in_conversation = false;
    view.social.partner = 0;
    view.social.perceived = {{2, Gender::Unknown, .5, 77}};
    auto payload = payload_for(NormPractice::Work, true);
    payload.evidence = {};
    view.norms_view.count = 1;
    view.norms_view.considered[0].key = payload.key;
    view.norm_context.effects[0].group_significance = .7;
    view.social.norms_view = view.norms_view;
    view.social.norm_context = view.norm_context;
    view.social.norm_options = {payload};
    Seed random;
    const auto choices = Planner::ideas(view, random);
    const auto found = std::find_if(choices.begin(), choices.end(), [](const auto& choice) {
        return choice.method == Method::Talk &&
               choice.interaction == Interaction::AskPractice && choice.partner == 2;
    });
    CHECK(found != choices.end());
    CHECK(found->norm_payload.key == payload.key);
}

TEST("norm_planner", work_option_carries_only_a_paid_work_key) {
    auto view = planner_view();
    view.known.fill(false);
    view.known[std::size_t(Method::Work)] = true;
    view.need[std::size_t(Metric::Money)] = .8;
    view.norms_view.count = 1;
    view.norms_view.considered[0].key =
        {practice_id(NormPractice::Work), 1, 1, 0, 0};
    Seed random{42, "0.7.0", catalogue_hash(), baseline_catalogue_hash()};
    const auto options = Planner::ideas(view, random);
    const auto found = std::find_if(options.begin(), options.end(), [](const auto& option) {
        return option.method == Method::Work;
    });
    CHECK(found != options.end());
    CHECK(found->norm_payload.present);
    CHECK(found->norm_payload.key == view.norms_view.considered[0].key);
}

TEST("norm_planner", work_forecast_uses_the_paid_approval_history) {
    auto view = planner_view();
    auto payload = payload_for(NormPractice::Work);
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = payload.key;
    prediction.approval_known = true;
    prediction.approval_coverage = 1;
    prediction.approve = 1;
    prediction.seen_known = true;
    prediction.seen = 1;
    prediction.own_revision = 19;
    view.norm_context.effects[0] = {5, 2, 1, 1, 1, 1, 0, true};
    PlanOption option{Method::Work, 7, 0, 0};
    option.norm_payload = payload;
    const auto approved = Planner::forecast(view, option);
    const auto* approved_term = find_term(approved, ConsequenceKind::SocialAcceptance, 1);
    CHECK(approved_term != nullptr);
    CHECK(approved_term->amount > 0);

    prediction.approve = 0;
    prediction.disapprove = 1;
    const auto disapproved = Planner::forecast(view, option);
    const auto* disapproved_term = find_term(disapproved, ConsequenceKind::SocialAcceptance, 1);
    CHECK(disapproved_term != nullptr);
    CHECK(disapproved_term->amount < 0);
    CHECK(approved.score > disapproved.score);
}

TEST("norm_planner", ask_money_carries_help_and_receiver_can_value_it) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    view.civil_enabled = true;
    view.help_people = {2};
    view.help_budget = 0;
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = {practice_id(NormPractice::Help), 1, 1, 0, 0};
    prediction.personal_principle_known = true;
    prediction.personal_resistance = .8;
    const auto choices = social_choices(view);
    const auto found = std::find_if(choices.begin(), choices.end(), [](const auto& choice) {
        return choice.kind == Interaction::AskMoney;
    });
    CHECK(found != choices.end());
    CHECK(found->norm_payload.present);
    CHECK(found->norm_payload.key.practice == practice_id(NormPractice::Help));

    auto receiver = view;
    receiver.help_budget = 36;
    receiver.norm_payload = {};
    const auto with_help = evaluate_social(receiver, Interaction::AskMoney, 2, 12, true);
    receiver.norms_view.considered[0].personal_principle_known = false;
    receiver.norms_view.considered[0].personal_resistance = 0;
    const auto without_help = evaluate_social(receiver, Interaction::AskMoney, 2, 12, true);
    CHECK(with_help.instrumental > without_help.instrumental);
    CHECK(with_help.score > without_help.score);
}

TEST("norm_planner", ask_money_receiver_counts_paid_help_approval_once) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    view.civil_enabled = true;
    view.help_budget = 36;
    view.norm_payload = {};
    view.norms_view.count = 1;
    auto& prediction = view.norms_view.considered[0];
    prediction.key = {practice_id(NormPractice::Help), 1, 1, 0, 0};
    prediction.approval_known = true;
    prediction.approve = .8;
    prediction.disapprove = .1;
    prediction.indifferent = .1;
    prediction.seen_known = true;
    prediction.seen = .5;
    view.norm_context.effects[0] = {7, 3, 1, 1, 1, 1, 0, false};
    const auto with_approval = evaluate_social(view, Interaction::AskMoney, 2, 12, true);
    prediction.approval_known = false;
    const auto without_approval = evaluate_social(view, Interaction::AskMoney, 2, 12, true);
    CHECK(with_approval.score > without_approval.score);

    view.norms_view.mode = NormMode::NoNormDecisionEffects;
    prediction.approval_known = true;
    const auto disabled = evaluate_social(view, Interaction::AskMoney, 2, 12, true);
    NEAR(disabled.score, without_approval.score, 1e-15);
}

TEST("norm_planner", privacy_uses_paid_variant_one_outside_legacy_mode) {
    auto base = planner_view().social;
    base.in_conversation = true;
    base.partner = 2;
    base.memory.community.enabled = true;
    Information information;
    information.id = 10;
    information.occurred_at = 0;
    information.confidence = 1;
    information.importance = .8;
    information.sensitivity = 1;
    information.disclosure = Disclosure::Entrusted;
    CHECK(base.memory.community.receive(information, 0, 55, 0));

    auto no_effects_low = base;
    auto no_effects_high = base;
    no_effects_low.norms_view.mode = NormMode::NoNormDecisionEffects;
    no_effects_high.norms_view.mode = NormMode::NoNormDecisionEffects;
    no_effects_low.memory.community.confidentiality = 0;
    no_effects_high.memory.community.confidentiality = 1;
    const auto low = evaluate_social(no_effects_low, Interaction::ShareNews, 2, 10);
    const auto high = evaluate_social(no_effects_high, Interaction::ShareNews, 2, 10);
    NEAR(low.moral, high.moral, 1e-15);

    auto enabled = base;
    enabled.memory.community.confidentiality = 0;
    const auto without_privacy = evaluate_social(enabled, Interaction::ShareNews, 2, 10);
    enabled.norms_view.count = 1;
    auto& privacy = enabled.norms_view.considered[0];
    privacy.key = {practice_id(NormPractice::Privacy), 0, 0, 0, 1};
    privacy.personal_principle_known = true;
    privacy.personal_resistance = .8;
    const auto with_privacy = evaluate_social(enabled, Interaction::ShareNews, 2, 10);
    CHECK(with_privacy.moral > without_privacy.moral);
    CHECK(with_privacy.score < without_privacy.score);
}

TEST("norm_planner", norm_interactions_have_complete_catalogue_entries) {
    CHECK(std::string(interaction_name(Interaction::AskPractice)) == "ask_practice");
    CHECK(std::string(interaction_name(Interaction::DisapprovePractice)) ==
          "disapprove_practice");
    CHECK(interaction_duration(Interaction::AskPractice) == 30000);
    CHECK(interaction_duration(Interaction::ExplainPractice) == 60000);
    CHECK(interaction_duration(Interaction::ApprovePractice) == 15000);
    CHECK(interaction_duration(Interaction::DisapprovePractice) == 15000);
    CHECK(catalogue_hash() ==
          "b1234046489f20c31a287285eec193adf61ef0db67db6b3a4c2ed356399cece9");
    CHECK(baseline_catalogue_hash() ==
          "51c399afffdc6177265574ab3750e3bdf7bc4f43e837a7623967e50779ba1f8b");
}

TEST("norm_planner", norm_interaction_is_not_blocked_by_life_or_civil_gates) {
    auto view = planner_view().social;
    view.life_enabled = false;
    view.civil_enabled = false;
    view.in_conversation = true;
    view.partner = 2;
    view.norm_payload = payload_for(NormPractice::Work, true);
    const auto evaluation =
        evaluate_social(view, Interaction::AskPractice, 2, 0);
    CHECK(evaluation.known);
}

TEST("norm_planner", explanation_flag_preserves_reported_approval_as_explanation) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    auto payload = payload_for(NormPractice::Honesty);
    payload.explanation = true;
    view.norms_view.count = 1;
    view.norms_view.considered[0].key = payload.key;
    view.norm_context.effects[0].group_significance = .7;
    view.norm_options = {payload};
    const auto choices = social_choices(view);
    CHECK(std::any_of(choices.begin(), choices.end(), [](const auto& choice) {
        return choice.kind == Interaction::ExplainPractice;
    }));
    CHECK(std::none_of(choices.begin(), choices.end(), [](const auto& choice) {
        return choice.kind == Interaction::ApprovePractice;
    }));
}

TEST("norm_planner", norm_reply_targets_the_payload_subject) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    auto payload = payload_for(NormPractice::Honesty);
    payload.subject = 3;
    view.norms_view.count = 1;
    view.norms_view.considered[0].key = payload.key;
    view.norm_context.effects[0].group_significance = 1;
    view.norm_options = {payload};
    const auto choices = social_choices(view);
    CHECK(std::none_of(choices.begin(), choices.end(), [](const auto& choice) {
                           return choice.kind == Interaction::ApprovePractice;
                       }));
}

TEST("norm_planner", norm_drafts_deduplicate_by_semantic_payload) {
    CivilMemory memory;
    CHECK(memory.learn_number(2, 200, 1, 0));
    LetterContent a;
    a.kind = LetterKind::NormPractice;
    a.norm_payload = payload_for(NormPractice::Honesty);
    LetterContent b = a;
    b.norm_payload.key.practice = practice_id(NormPractice::Promise);
    b.norm_payload.evidence.key = b.norm_payload.key;
    CHECK(memory.compose(2, a, 50, 0) != 0);
    CHECK(memory.compose(2, b, 50, 0) != 0);
    CHECK(memory.drafts.size() == 2);
}

TEST("norm_planner", no_effects_forecast_keeps_disclosure_sensitivity_cost) {
    auto high = planner_view();
    high.norms_view.mode = NormMode::NoNormDecisionEffects;
    high.social.norms_view.mode = NormMode::NoNormDecisionEffects;
    high.social.in_conversation = true;
    high.social.partner = 2;
    high.social.memory.community.enabled = true;
    Information information;
    information.id = 10;
    information.confidence = 1;
    information.importance = .8;
    information.sensitivity = 1;
    information.disclosure = Disclosure::Entrusted;
    CHECK(high.social.memory.community.receive(information, 0, 55, 0));
    high.social.memory.revision = 41;
    auto low = high;
    low.social.memory.community.entries.front().content.sensitivity = 0;
    PlanOption option{Method::Social, 7, 2, 0};
    option.interaction = Interaction::ShareNews;
    option.object = 10;
    const auto high_cost = Planner::forecast(high, option);
    const auto low_cost = Planner::forecast(low, option);
    CHECK(high_cost.path.size() == 1);
    CHECK(low_cost.path.size() == 1);
    CHECK(high_cost.score < low_cost.score);
    const auto practical = std::find_if(
        high_cost.norm_ledger.begin(), high_cost.norm_ledger.end(),
        [](const auto& term) {
            return term.key.kind == ConsequenceKind::ResidualUncertainty &&
                   term.owner == ForecastOwner::Procedure &&
                   term.knownness == ConsequenceKnownness::Assumed;
        });
    CHECK(practical != high_cost.norm_ledger.end());
    NEAR(practical->amount, -.15, 1e-15);
    CHECK(practical->source_revision == high.social.memory.revision);
    CHECK(practical->norm_key == NormKey{});
}

TEST("norm_planner", enabled_share_news_ledger_uses_paid_privacy_revision) {
    auto view = planner_view();
    view.social.in_conversation = true;
    view.social.partner = 2;
    view.social.memory.community.enabled = true;
    Information information;
    information.id = 10;
    information.confidence = 1;
    information.importance = .8;
    information.sensitivity = 0;
    information.disclosure = Disclosure::Entrusted;
    CHECK(view.social.memory.community.receive(information, 0, 55, 0));
    view.norms_view.count = 1;
    auto& privacy = view.norms_view.considered[0];
    privacy.key = {practice_id(NormPractice::Privacy), 0, 0, 0, 1};
    privacy.personal_principle_known = true;
    privacy.personal_resistance = .8;
    privacy.own_revision = 77;
    PlanOption option{Method::Social, 7, 2, 0};
    option.interaction = Interaction::ShareNews;
    option.object = 10;
    const auto decision = Planner::forecast(view, option);
    const auto* principle =
        find_term(decision, ConsequenceKind::PersonalPrinciple, 0);
    CHECK(principle != nullptr);
    CHECK(principle->key.object == practice_id(NormPractice::Privacy));
    CHECK(principle->source_revision == privacy.own_revision);
    CHECK(principle->norm_key == privacy.key);
}

TEST("norm_planner", no_effects_excludes_inherited_public_intimacy_cost) {
    auto view = planner_view().social;
    view.norms_view.mode = NormMode::NoNormDecisionEffects;
    view.life_enabled = true;
    view.in_conversation = true;
    view.partner = 2;
    view.known_audience = 3;
    const auto evaluation =
        evaluate_social(view, Interaction::PartnerIntimacy, 2, 0);
    CHECK(evaluation.known);
    NEAR(evaluation.moral, 0, 1e-15);

    view.norms_view.mode = NormMode::Enabled;
    const auto enabled =
        evaluate_social(view, Interaction::PartnerIntimacy, 2, 0);
    NEAR(enabled.moral, 1, 1e-15);
    NEAR(enabled.assumed_normative_moral, 1, 1e-15);
    CHECK(enabled.assumed_normative_source_version == 1);
    NEAR(enabled.practical_moral, 0, 1e-15);

    auto planner = planner_view();
    planner.social.life_enabled = true;
    planner.social.known_audience = 3;
    PlanOption option{Method::Social, 7, 2, 0};
    option.interaction = Interaction::PartnerIntimacy;
    const auto decision = Planner::forecast(planner, option);
    const auto inherited = std::find_if(
        decision.norm_ledger.begin(), decision.norm_ledger.end(),
        [](const auto& term) {
            return term.key.kind == ConsequenceKind::PersonalPrinciple &&
                   term.owner == ForecastOwner::Procedure;
        });
    CHECK(inherited != decision.norm_ledger.end());
    NEAR(inherited->amount, -1, 1e-15);
    CHECK(inherited->knownness == ConsequenceKnownness::Assumed);
    CHECK(inherited->source_revision == 1);
    CHECK(inherited->norm_key == NormKey{});
}

TEST("norm_planner", asocial_principle_ledger_uses_paid_key_and_revision) {
    auto view = planner_view();
    view.norms_view.count = 1;
    auto& property = view.norms_view.considered[0];
    property.key = {practice_id(NormPractice::Property), 0, 0, 0, 1};
    property.personal_principle_known = true;
    property.personal_resistance = .6;
    property.own_revision = 88;
    PlanOption option{Method::TakeFood, 7, 0, 0};
    const auto decision = Planner::forecast(view, option);
    const auto* principle =
        find_term(decision, ConsequenceKind::PersonalPrinciple, 0);
    CHECK(principle != nullptr);
    CHECK(principle->key.object == practice_id(NormPractice::Property));
    CHECK(principle->source_revision == property.own_revision);
    CHECK(principle->norm_key == property.key);
}

TEST("norm_planner", romantic_moral_keeps_four_paid_principle_sources) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    view.known_audience = 3;
    view.memory.gender = Gender::Woman;
    PersonBelief partner;
    partner.id = 2;
    partner.gender = Gender::Woman;
    partner.known_kin = true;
    view.memory.people = {partner};
    view.memory.community.enabled = true;
    view.familiarities = {{2, 0}};
    view.norms_view.count = 4;
    const std::array<NormPractice, 4> practices{
        NormPractice::PersonalRomance, NormPractice::Privacy,
        NormPractice::StrangerRomance, NormPractice::KinRomance};
    for (std::size_t i = 0; i < practices.size(); ++i) {
        auto& prediction = view.norms_view.considered[i];
        prediction.key = {practice_id(practices[i]), 0, 0, 0, 0};
        prediction.personal_principle_known = true;
        prediction.personal_resistance = .1 * double(i + 1);
        prediction.own_revision = 71 + i;
    }
    const auto evaluation =
        evaluate_social(view, Interaction::RomanticTouch, 2, 0);
    CHECK(evaluation.normative_effect_count == 4);
    for (std::size_t i = 0; i < practices.size(); ++i) {
        const auto found = std::find_if(
            evaluation.normative_effects.begin(),
            evaluation.normative_effects.begin() +
                evaluation.normative_effect_count,
            [&](const auto& component) {
                return component.key.practice == practice_id(practices[i]);
            });
        CHECK(found != evaluation.normative_effects.begin() +
                           evaluation.normative_effect_count);
        CHECK(found->source_revision == 71 + i);
    }
    double component_total = evaluation.practical_moral +
                             evaluation.assumed_normative_moral;
    for (std::size_t i = 0; i < evaluation.normative_effect_count; ++i)
        component_total += evaluation.normative_effects[i].moral_cost;
    NEAR(component_total, evaluation.moral, 1e-15);
}

TEST("norm_planner", receiving_boundary_repetition_keeps_paid_source) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    ContextExperience past;
    past.kind = Interaction::FriendlyTouch;
    past.person = 2;
    past.incoming = 2;
    past.issued_boundaries = 1;
    view.memory.experiences = {past};
    view.norms_view.count = 1;
    auto& boundary = view.norms_view.considered[0];
    boundary.key = {practice_id(NormPractice::Boundary), 0, 0, 0, 0};
    boundary.personal_principle_known = true;
    boundary.personal_resistance = .8;
    boundary.own_revision = 93;
    const auto evaluation =
        evaluate_social(view, Interaction::FriendlyTouch, 2, 0, true);
    CHECK(evaluation.normative_effect_count == 1);
    CHECK(evaluation.normative_effects[0].key == boundary.key);
    CHECK(evaluation.normative_effects[0].source_revision ==
          boundary.own_revision);
    NEAR(evaluation.normative_effects[0].repetition_cost, .24, 1e-15);
    NEAR(evaluation.repetition, .24, 1e-15);
}

TEST("norm_planner", public_intimacy_override_keeps_boundary_repetition_source) {
    auto view = planner_view().social;
    view.life_enabled = true;
    view.in_conversation = true;
    view.partner = 2;
    view.known_audience = 3;
    ContextExperience past;
    past.kind = Interaction::PartnerIntimacy;
    past.person = 2;
    past.public_context = true;
    past.incoming = 2;
    past.issued_boundaries = 1;
    view.memory.experiences = {past};
    view.norms_view.count = 1;
    auto& boundary = view.norms_view.considered[0];
    boundary.key = {practice_id(NormPractice::Boundary), 0, 0, 0, 0};
    boundary.personal_principle_known = true;
    boundary.personal_resistance = .8;
    boundary.own_revision = 95;
    const auto enabled =
        evaluate_social(view, Interaction::PartnerIntimacy, 2, 0, true);
    NEAR(enabled.moral, 1, 1e-15);
    NEAR(enabled.repetition, .24, 1e-15);
    CHECK(enabled.normative_effect_count == 1);
    CHECK(enabled.normative_effects[0].key == boundary.key);
    CHECK(enabled.normative_effects[0].source_revision == boundary.own_revision);
    NEAR(enabled.normative_effects[0].moral_cost, 0, 1e-15);
    NEAR(enabled.normative_effects[0].repetition_cost, .24, 1e-15);

    view.norms_view.mode = NormMode::NoNormDecisionEffects;
    const auto no_effects =
        evaluate_social(view, Interaction::PartnerIntimacy, 2, 0, true);
    NEAR(no_effects.moral, 0, 1e-15);
    CHECK(no_effects.normative_effect_count == 0);
}

TEST("norm_planner", receiving_help_instrumental_keeps_paid_source) {
    auto view = planner_view().social;
    view.in_conversation = true;
    view.partner = 2;
    view.civil_enabled = true;
    view.help_budget = 36;
    view.norms_view.count = 1;
    auto& help = view.norms_view.considered[0];
    help.key = {practice_id(NormPractice::Help), 1, 1, 0, 0};
    help.personal_principle_known = true;
    help.personal_resistance = .7;
    help.own_revision = 94;
    const auto evaluation =
        evaluate_social(view, Interaction::AskMoney, 2, 12, true);
    CHECK(evaluation.normative_effect_count == 1);
    CHECK(evaluation.normative_effects[0].key == help.key);
    CHECK(evaluation.normative_effects[0].source_revision == help.own_revision);
    NEAR(evaluation.normative_effects[0].instrumental_value, .7, 1e-15);
}

TEST("norm_planner", clipped_privacy_and_practical_cost_keep_one_total) {
    auto view = planner_view();
    view.social.in_conversation = true;
    view.social.partner = 2;
    view.social.memory.community.enabled = true;
    view.social.memory.revision = 42;
    Information information;
    information.id = 10;
    information.confidence = 1;
    information.importance = .8;
    information.sensitivity = 1;
    information.disclosure = Disclosure::Entrusted;
    CHECK(view.social.memory.community.receive(information, 0, 55, 0));
    view.norms_view.count = 1;
    auto& privacy = view.norms_view.considered[0];
    privacy.key = {practice_id(NormPractice::Privacy), 0, 0, 0, 1};
    privacy.personal_principle_known = true;
    privacy.personal_resistance = .9;
    privacy.own_revision = 97;
    PlanOption option{Method::Social, 7, 2, 0};
    option.interaction = Interaction::ShareNews;
    option.object = 10;
    const auto decision = Planner::forecast(view, option);
    NEAR(decision.moral, 1, 1e-15);
    NEAR(decision.social_evaluation.normative_effects[0].moral_cost +
             decision.social_evaluation.practical_moral,
         1, 1e-15);
    const auto principle = std::find_if(
        decision.norm_ledger.begin(), decision.norm_ledger.end(),
        [](const auto& term) {
            return term.key.kind == ConsequenceKind::PersonalPrinciple &&
                   term.owner == ForecastOwner::NormPrior;
        });
    const auto practical = std::find_if(
        decision.norm_ledger.begin(), decision.norm_ledger.end(),
        [](const auto& term) {
            return term.key.kind == ConsequenceKind::ResidualUncertainty &&
                   term.owner == ForecastOwner::Procedure;
        });
    CHECK(principle != decision.norm_ledger.end());
    CHECK(practical != decision.norm_ledger.end());
    NEAR(principle->amount + practical->amount, -1, 1e-15);
}

TEST("norm_planner", malformed_serialized_normative_components_are_rejected) {
    SocialEvaluation overflow;
    overflow.normative_effect_count =
        std::uint8_t(overflow.normative_effects.size() + 1);
    bool rejected_overflow = false;
    try {
        validate_social_evaluation(overflow);
    } catch (const std::invalid_argument&) {
        rejected_overflow = true;
    }
    CHECK(rejected_overflow);

    SocialEvaluation nonfinite;
    nonfinite.normative_effect_count = 1;
    nonfinite.normative_effects[0].key.practice =
        practice_id(NormPractice::Privacy);
    nonfinite.normative_effects[0].source_revision = 1;
    nonfinite.normative_effects[0].moral_cost =
        std::numeric_limits<double>::quiet_NaN();
    bool rejected_nonfinite = false;
    try {
        validate_social_evaluation(nonfinite);
    } catch (const std::invalid_argument&) {
        rejected_nonfinite = true;
    }
    CHECK(rejected_nonfinite);
}
