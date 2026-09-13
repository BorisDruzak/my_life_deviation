#include "test.hpp"

#include "life/archive.hpp"
#include "life/decision_ledger.hpp"
#include "life/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

using namespace life;

namespace {

World conformance_world(NormMode mode=NormMode::Enabled) {
    auto world=World::generate(42,8);
    NormProfile profile;
    profile.mode=mode;
    world.configure_norm_memory(profile);
    return world;
}

NormKey conformance_key(std::uint32_t variant=1,Id group=0,std::uint32_t context=0) {
    return {practice_id(NormPractice::Help),group,context,0,variant};
}

NormSource conformance_source(std::uint64_t id,NormOrigin origin=NormOrigin::Observed,
                              std::uint64_t revision=1) {
    return {id,id+20'000,revision,2,origin,true};
}

NormObservation conformance_observation(std::uint64_t delivery,NormKey key,
                                        NormChannel channel=NormChannel::Descriptive,
                                        double value=1) {
    NormObservation observation;
    observation.key=key;
    observation.source=conformance_source(delivery);
    observation.at=0;
    observation.observed_actor=2;
    observation.quality=1;
    observation.confidence=1;
    observation.reliability=1;
    observation.dose=1;
    observation.value=value;
    observation.channel=channel;
    observation.applicable=true;
    observation.value_known=true;
    observation.outcome_window_complete=true;
    return observation;
}

template<class Predicate>
bool conformance_run_until(World& world,Predicate predicate,unsigned iterations=12'000) {
    for(unsigned i=0;i<iterations;++i) {
        if(predicate()) return true;
        world.run_ms(25);
    }
    return predicate();
}

template<class Value>
std::string conformance_bytes(const Value& value) {
    Writer writer;
    writer(value);
    return writer.data;
}

void authored_group(Actor& actor,Id group,std::uint32_t context,double significance) {
    actor.mind.norm_context.groups.clear();
    actor.mind.norm_context.goals.clear();
    actor.mind.norm_context.questions.clear();
    actor.mind.norm_context.groups.push_back(
        {group,context,actor.place,95'001,{{actor.id,95'002,0}}});
    actor.mind.norm_context.goals.push_back(
        {95'003,95'004,group,significance,1,true});
    ++actor.mind.norm_context.revision;
}

const NormQuestion* question_for(const Actor& actor,const NormKey& key) {
    const auto& questions=actor.mind.norm_context.questions;
    const auto found=std::find_if(questions.begin(),questions.end(),
        [&](const auto& question){return question.key==key;});
    return found==questions.end()?nullptr:&*found;
}

OutcomeSignal successful_personal_outcome(std::uint64_t id) {
    OutcomeSignal signal;
    signal.id=signal.root=signal.action=id;
    signal.method=Method::Talk;
    signal.completed=true;
    signal.feedback_observed=true;
    signal.directness=1;
    signal.intentional=true;
    signal.at=0;
    signal.goal_importance=.8;
    signal.observed_mask=1u<<5;
    signal.observed[5]=.4;
    signal.action_feedback=.9;
    signal.acceptance_observed=true;
    signal.accepted=1;
    return signal;
}

PersonalView normative_plan_view() {
    PersonalView view;
    view.self=1;
    view.place=view.home=7;
    view.now=10*3'600'000;
    view.capability.gate=1;
    view.capability.context=12;
    view.capability.operations=64;
    view.capability.alternatives=8;
    view.known.fill(true);
    KnownPlace place;
    place.id=7;
    place.services=0xffffffffu;
    place.food=Truth::Confirmed;
    place.price=12;
    view.places.push_back(place);
    view.norms_view.mode=NormMode::Enabled;
    view.self_enabled=true;
    view.civil.enabled=true;
    view.civil.memory.enabled=true;
    view.civil.profile.budget_policy=BudgetPolicy::Deliberative;
    view.social.enabled=true;
    view.social.civil_enabled=true;
    view.social.self=view.self;
    view.social.place=view.place;
    view.social.now=view.now;
    view.social.in_conversation=true;
    view.social.partner=2;
    view.social.perceived={{2,Gender::Unknown,.5,70'002}};
    for(auto& method:view.social.memory.methods)
        method={.9,0,.8,.8,70'003};
    return view;
}

NormPayload normative_payload(const NormKey& key,bool question=false) {
    NormPayload payload;
    payload.present=true;
    payload.question=question;
    payload.key=key;
    payload.subject=1;
    payload.evidence.key=key;
    payload.evidence.channel=NormChannel::Approval;
    payload.evidence.approval=ApprovalValue::Approve;
    payload.evidence.value=1;
    payload.evidence.value_known=true;
    return payload;
}

void add_clothing_shop(PersonalView& view) {
    view.civil.memory.shops.push_back({7,71'000,0,0,{{10,35,0},{11,95,1},{12,180,2}}});
    auto& question=view.civil.memory.questions.ensure(QuestionKind::Clothing,7,71'001,0);
    question.pending=false;
    question.actionable=true;
}

} // namespace

TEST("norm_conformance",nm019_hidden_object_cannot_change_same_accessible_learning) {
    auto visible=conformance_world();
    auto hidden=visible;
    for(auto* world:{&visible,&hidden}) {
        auto& actor=world->edit_for_test().actors[0];
        actor.mind.norm_context.groups.clear();
        actor.mind.norm_context.goals.clear();
        ++actor.mind.norm_context.revision;
        actor.cog.percepts.clear();
    }
    auto& hidden_state=hidden.edit_for_test();
    auto& hidden_actor=hidden_state.actors[7];
    auto garment=std::find_if(hidden_state.social.objects.begin(),hidden_state.social.objects.end(),
        [&](const auto& object){return object.id==hidden_actor.equipment.garment;});
    CHECK(garment!=hidden_state.social.objects.end());
    garment->condition=.05;
    hidden_actor.mind.civil.garment_condition=.05;

    const auto key=conformance_key(19);
    auto argument=conformance_observation(919'000,key,NormChannel::PersonalPrinciple,.25);
    argument.accepted_argument=true;
    argument.plasticity=1;
    visible.norm_observation_for_test(1,argument);
    hidden.norm_observation_for_test(1,argument);
    CHECK(conformance_run_until(visible,[&]{
        return visible.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    CHECK(conformance_run_until(hidden,[&]{
        return hidden.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    CHECK(conformance_bytes(visible.state().actors[0].mind.norm_memory)==
          conformance_bytes(hidden.state().actors[0].mind.norm_memory));
}

TEST("norm_conformance",nm023_budget_one_keeps_the_record_for_later) {
    auto world=conformance_world();
    const auto key=conformance_key(23);
    constexpr std::uint64_t delivery=923'000;
    world.norm_observation_for_test(1,conformance_observation(delivery,key));
    auto& actor=world.edit_for_test().actors[0];
    auto& cognition=actor.cog;
    cognition.active=true;
    cognition.operation=Operation::None;
    cognition.focus=(11ull<<48)+delivery;
    cognition.focus_kind=TopicKind::Memory;
    cognition.focus_metric=5;
    cognition.focus_basis=delivery;
    cognition.focus_priority=1;
    cognition.focus_since=0;
    cognition.rebuild_until=0;
    cognition.review_at=0;
    cognition.budget=1;
    cognition.spent=0;
    world.run_ms(25);
    const auto& after=world.state().actors[0];
    CHECK(after.cog.norm.inbox.size()==1);
    CHECK(after.mind.norm_memory.record_revision(key)==0);
    CHECK(after.cog.norm.integrated==0);
    CHECK(after.cog.spent==0);
}

TEST("norm_conformance",nm026_observed_no_reaction_is_evidence_not_morality) {
    NormMemory memory;
    const auto key=conformance_key(26);
    auto no_reaction=conformance_observation(926'000,key,NormChannel::Reaction,0);
    no_reaction.outcome_window_complete=true;
    no_reaction.outcome_severity_known=false;
    CHECK(memory.apply(no_reaction,0)==ApplyEvidence::Added);
    const auto prediction=memory.predict(key,0);
    CHECK(prediction.reacted_known);
    CHECK(prediction.reacted<.5);
    CHECK(!prediction.approval_known);
    CHECK(!prediction.personal_principle_known);
    CHECK(!prediction.sanction_known);
}

TEST("norm_conformance",nm027_absent_institution_produces_no_negative_sanction_evidence) {
    auto world=conformance_world();
    const auto key=conformance_key(27,927,27);
    world.run_seconds(2*86'400);
    const auto& memory=world.state().actors[0].mind.norm_memory;
    CHECK(memory.record_revision(key)==0);
    const auto prediction=memory.predict(key,world.state().now);
    CHECK(!prediction.seen_known);
    CHECK(!prediction.classified_known);
    CHECK(!prediction.reacted_known);
    CHECK(!prediction.severity_known);
}

TEST("norm_conformance",nm030_own_observed_outcome_can_update_self_norms_cannot) {
    auto world=conformance_world();
    const auto before=conformance_bytes(world.state().actors[0].mind.self);
    world.observe_outcome_for_test(1,successful_personal_outcome(930'000));
    CHECK(conformance_run_until(world,[&]{
        return world.state().actors[0].cog.self_integrations>0;
    }));
    CHECK(conformance_bytes(world.state().actors[0].mind.self)!=before);
}

TEST("norm_conformance",nm067_same_revision_does_not_create_or_wake_another_question) {
    auto world=conformance_world();
    auto& actor=world.edit_for_test().actors[0];
    authored_group(actor,67,167,1);
    actor.mind.civil.garment_condition=0;
    const NormKey key{practice_id(NormPractice::ClothingCondition),67,167,0,0};
    actor.mind.norm_memory.seed_approval(key,{0,1,0},8,
        conformance_source(967'000,NormOrigin::LegacyPrior),0);
    auto evaluations=actor.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(conformance_run_until(world,[&]{
        return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
    }));
    const auto& first_actor=world.state().actors[0];
    const auto* first=question_for(first_actor,key);
    CHECK(first!=nullptr);
    const auto id=first->id;
    const auto revision=first->revision;
    const auto woken=first_actor.mind.norm_context.questions_woken;

    evaluations=first_actor.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(conformance_run_until(world,[&]{
        return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
    }));
    const auto& after=world.state().actors[0];
    const auto* repeated=question_for(after,key);
    CHECK(after.mind.norm_context.questions.size()==1);
    CHECK(repeated!=nullptr&&repeated->id==id&&repeated->revision==revision);
    CHECK(after.mind.norm_context.questions_woken==woken);
}

TEST("norm_conformance",nm068_new_revision_reopens_only_its_linked_question) {
    auto world=conformance_world();
    auto& actor=world.edit_for_test().actors[0];
    authored_group(actor,68,168,1);
    actor.mind.civil.garment_condition=0;
    actor.mind.civil.garment_tier=0;
    const NormKey condition{practice_id(NormPractice::ClothingCondition),68,168,0,0};
    // A categorical disapproval concerns the tier the actor actually wears.
    const NormKey tier{practice_id(NormPractice::ClothingTier),68,168,0,0};
    auto condition_source=conformance_source(968'001,NormOrigin::LegacyPrior);
    actor.mind.norm_memory.seed_approval(condition,{0,1,0},8,condition_source,0);
    actor.mind.norm_memory.seed_approval(tier,{0,1,0},8,
        conformance_source(968'002,NormOrigin::LegacyPrior),0);
    auto evaluations=actor.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(conformance_run_until(world,[&]{
        return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
    }));
    auto& authored=world.edit_for_test().actors[0];
    auto* condition_question=const_cast<NormQuestion*>(question_for(authored,condition));
    auto* tier_question=const_cast<NormQuestion*>(question_for(authored,tier));
    CHECK(condition_question!=nullptr&&tier_question!=nullptr);
    condition_question->pending=false;
    condition_question->waiting=true;
    tier_question->pending=false;
    tier_question->waiting=true;
    const auto tier_revision=tier_question->revision;
    ++authored.mind.norm_context.revision;
    condition_source.revision=2;
    authored.mind.norm_memory.seed_approval(condition,{.1,.9,0},8,condition_source,0);

    evaluations=authored.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(conformance_run_until(world,[&]{
        return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
    }));
    const auto& after=world.state().actors[0];
    condition_question=const_cast<NormQuestion*>(question_for(after,condition));
    tier_question=const_cast<NormQuestion*>(question_for(after,tier));
    CHECK(condition_question!=nullptr&&condition_question->pending&&!condition_question->waiting);
    CHECK(tier_question!=nullptr&&!tier_question->pending&&tier_question->waiting);
    CHECK(tier_question->revision==tier_revision);
}

TEST("norm_conformance",nm072_hidden_job_change_does_not_rewrite_known_membership) {
    auto world=conformance_world();
    auto& state=world.edit_for_test();
    state.autonomy=false;
    auto& observer=state.actors[0];
    authored_group(observer,72,172,1);
    const auto before=conformance_bytes(observer.mind.norm_context);
    auto& hidden=state.actors[1];
    const auto organization=hidden.employment.organization;
    hidden.employment.active=false;
    auto known=std::find_if(state.life.organizations.begin(),state.life.organizations.end(),
        [&](const auto& value){return value.id==organization;});
    CHECK(known!=state.life.organizations.end());
    auto member=std::find_if(known->members.begin(),known->members.end(),
        [&](const auto& value){return value.person==hidden.id;});
    CHECK(member!=known->members.end());
    member->active=false;
    world.run_seconds(120);
    CHECK(conformance_bytes(world.state().actors[0].mind.norm_context)==before);
}

TEST("norm_conformance",nm074_self_history_changes_residual_not_the_norm_prediction) {
    auto low_view=normative_plan_view();
    NormPrediction prediction;
    prediction.key=conformance_key(74);
    prediction.approval_known=true;
    prediction.approve=.6;
    prediction.disapprove=.3;
    prediction.indifferent=.1;
    prediction.approval_coverage=.5;
    prediction.own_revision=74;
    low_view.norms_view.count=1;
    low_view.norms_view.considered[0]=prediction;
    low_view.norm_context.effects[0]={74,2,1,1,1,1,0,true};
    low_view.social.norms_view=low_view.norms_view;
    low_view.social.norm_context=low_view.norm_context;
    for(auto& self:low_view.self_predictions) {
        self.acceptance=.1;
        self.axis_confidence[std::size_t(SelfAxis::Acceptance)]=1;
    }
    auto high_view=low_view;
    for(auto& self:high_view.self_predictions) self.acceptance=.9;
    PlanOption option;
    option.method=Method::Social;
    option.place=7;
    option.partner=2;
    option.interaction=Interaction::ApprovePractice;
    option.norm_payload=normative_payload(prediction.key);
    const auto low_decision=Planner::forecast(low_view,option);
    const auto high_decision=Planner::forecast(high_view,option);
    CHECK(!low_decision.path.empty()&&!high_decision.path.empty());
    CHECK(std::abs(low_decision.norm_raw-high_decision.norm_raw)>1e-6);
    CHECK(std::abs(low_decision.score-high_decision.score)>1e-6);
    CHECK(conformance_bytes(low_view.norms_view.considered[0])==
          conformance_bytes(high_view.norms_view.considered[0]));

    const double low=blend_social_outcome_probability(0,0,prediction.approve,
                                                       prediction.approval_coverage,.1,1);
    const double high=blend_social_outcome_probability(0,0,prediction.approve,
                                                        prediction.approval_coverage,.9,1);
    CHECK(high>low);
}

TEST("norm_conformance",nm075_bootstrap_numeric_priors_have_explicit_sources) {
    auto world=conformance_world();
    const auto& memory=world.state().actors[0].mind.norm_memory;
    const NormKey property{practice_id(NormPractice::Property),0,0,0,1};
    const auto prediction=memory.predict(property,0);
    CHECK(prediction.personal_principle_known);
    CHECK(prediction.personal_origin==NormOrigin::LegacyPrior);
    CHECK(prediction.seen_known&&prediction.classified_known&&prediction.reacted_known);
    CHECK(prediction.severity_known&&prediction.sanction_known);
    const auto& records=memory.records();
    const auto record=std::find_if(records.begin(),records.end(),
        [&](const auto& value){return value.key==property;});
    CHECK(record!=records.end());
    CHECK(record->personal.has_explicit_prior&&record->personal.prior_source.known_root!=0);
    CHECK(record->prior_source_known[std::size_t(NormChannel::Detection)]);
    CHECK(record->prior_source_known[std::size_t(NormChannel::Classification)]);
    CHECK(record->prior_source_known[std::size_t(NormChannel::Reaction)]);
    CHECK(record->severity_prior_source_known);
}

TEST("norm_conformance",nm076_frequency_and_disapproval_can_both_be_high) {
    NormMemory memory;
    const auto key=conformance_key(76);
    for(std::uint64_t i=0;i<12;++i) {
        auto occurrence=conformance_observation(976'000+i,key,NormChannel::Descriptive,1);
        occurrence.observed_actor=100+Id(i);
        memory.apply(occurrence,0);
        auto disapproval=conformance_observation(976'100+i,key,NormChannel::Approval,1);
        disapproval.approval=ApprovalValue::Disapprove;
        memory.apply(disapproval,0);
    }
    const auto prediction=memory.predict(key,0);
    CHECK(prediction.prevalence>.85);
    CHECK(prediction.disapprove>.75);
    CHECK(prediction.descriptive_known&&prediction.approval_known);
}

TEST("norm_conformance",nm077_relearning_is_local_and_gradual) {
    auto world=conformance_world();
    auto& memory=world.edit_for_test().actors[0].mind.norm_memory;
    const auto changing=conformance_key(77);
    const auto stable=conformance_key(78);
    memory.seed_principle(changing,.9,conformance_source(977'000,NormOrigin::LegacyPrior),0);
    memory.seed_principle(stable,.7,conformance_source(977'001,NormOrigin::LegacyPrior),0);
    const auto integrated=world.state().actors[0].cog.norm.integrated;
    for(std::uint64_t i=0;i<4;++i) {
        auto argument=conformance_observation(977'100+i,changing,
                                              NormChannel::PersonalPrinciple,0);
        argument.accepted_argument=true;
        argument.plasticity=1;
        world.norm_observation_for_test(1,argument);
    }
    CHECK(conformance_run_until(world,[&]{
        return world.state().actors[0].cog.norm.integrated>=integrated+4;
    }));
    const auto changed=world.state().actors[0].mind.norm_memory.predict(changing,world.state().now);
    const auto untouched=world.state().actors[0].mind.norm_memory.predict(stable,world.state().now);
    CHECK(changed.principle_weight<.9&&changed.principle_weight>.8);
    NEAR(untouched.principle_weight,.7,1e-12);
}

TEST("norm_conformance",nm059_tension_has_no_immediate_physical_or_self_effect) {
    auto world=conformance_world();
    auto& actor=world.edit_for_test().actors[0];
    actor.action={};
    authored_group(actor,59,159,1);
    actor.mind.civil.garment_condition=0;
    const NormKey key{practice_id(NormPractice::ClothingCondition),59,159,0,0};
    actor.mind.norm_memory.seed_approval(key,{0,1,0},8,
        conformance_source(959'000,NormOrigin::LegacyPrior),0);
    auto control=world;
    control.edit_for_test().actors[0].mind.norm_context.goals.front().importance=0;
    ++control.edit_for_test().actors[0].mind.norm_context.revision;
    const auto evaluations=actor.cog.norm.deep_evaluations;
    const auto control_evaluations=control.state().actors[0].cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    control.norm_context_for_test(1);
    bool completed=false;
    for(unsigned i=0;i<12'000&&!completed;++i) {
        world.run_ms(25);
        control.run_ms(25);
        completed=world.state().actors[0].cog.norm.deep_evaluations>evaluations&&
                  control.state().actors[0].cog.norm.deep_evaluations>control_evaluations;
    }
    CHECK(completed);
    const auto& after=world.state().actors[0];
    const auto& unchanged=control.state().actors[0];
    CHECK(question_for(after,key)!=nullptr);
    CHECK(question_for(unchanged,key)==nullptr);
    NEAR(after.money,unchanged.money,0);
    CHECK(after.food==unchanged.food);
    CHECK(conformance_bytes(after.equipment)==conformance_bytes(unchanged.equipment));
    CHECK(conformance_bytes(after.body)==conformance_bytes(unchanged.body));
    CHECK(conformance_bytes(after.mind.self)==conformance_bytes(unchanged.mind.self));
    CHECK(after.cog.self_integrations==unchanged.cog.self_integrations);
}

TEST("norm_conformance",nm062_unaffordable_norm_goal_waits_while_earning_remains_available) {
    auto world=conformance_world();
    auto& state=world.edit_for_test();
    auto& actor=state.actors[0];
    actor.action={};
    state.ledger.initial_money-=actor.money;
    actor.money=actor.mind.believed_money=0;
    actor.mind.civil.garment_condition=0;
    actor.mind.civil.garment_tier=0;
    authored_group(actor,62,162,1);
    const NormKey key{practice_id(NormPractice::ClothingCondition),62,162,0,0};
    actor.mind.norm_memory.seed_approval(key,{0,1,0},8,
        conformance_source(962'000,NormOrigin::LegacyPrior),0);
    const auto purchases=world.state().civil.purchases;
    const auto bought=actor.mind.civil.clothes_bought;
    const auto evaluations=actor.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(conformance_run_until(world,[&]{
        return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
    }));
    CHECK(question_for(world.state().actors[0],key)!=nullptr);
    world.run_seconds(30);
    const auto* waiting=question_for(world.state().actors[0],key);
    CHECK(waiting!=nullptr&&waiting->active);
    CHECK(world.state().civil.purchases==purchases);
    CHECK(world.state().actors[0].mind.civil.clothes_bought==bought);

    auto view=normative_plan_view();
    view.social.enabled=false;
    view.money=0;
    view.need.fill(0);
    view.need[7]=.8;
    view.civil.memory.garment_condition=0;
    view.civil.memory.desired_tier=2;
    add_clothing_shop(view);
    view.norms_view.count=1;
    view.norms_view.considered[0].key=key;
    view.norm_context.question_active=true;
    view.norm_context.question_key=key;
    Seed random{42,"0.7.0",catalogue_hash(),baseline_catalogue_hash()};
    const auto options=Planner::ideas(view,random);
    CHECK(std::none_of(options.begin(),options.end(),[](const auto& option){
        return option.method==Method::BuyClothes;
    }));
    CHECK(std::any_of(options.begin(),options.end(),[](const auto& option){
        return option.method==Method::Work;
    }));
}

TEST("norm_conformance",nm065_normative_purchase_has_one_future_buffer_term) {
    auto view=normative_plan_view();
    view.money=220;
    view.food=0;
    view.civil.memory.garment_condition=.2;
    view.civil.memory.garment_tier=0;
    view.civil.memory.desired_tier=1;
    view.civil.budget.protected_cash=200;
    view.civil.profile.risk_importance=1;
    add_clothing_shop(view);
    const NormKey key{practice_id(NormPractice::ClothingTier),1,1,0,1};
    view.norms_view.count=1;
    view.norms_view.considered[0].key=key;
    view.norms_view.considered[0].personal_principle_known=true;
    view.norms_view.considered[0].personal_resistance=.2;
    view.social.norms_view=view.norms_view;
    const auto options=civil_options(view);
    const auto selected=std::find_if(options.begin(),options.end(),[](const auto& option){
        return option.method==Method::BuyClothes&&option.object==11;
    });
    CHECK(selected!=options.end());
    const auto decision=Planner::forecast(view,*selected);
    CHECK(!decision.path.empty());
    const auto future_buffers=std::count_if(decision.norm_ledger.begin(),decision.norm_ledger.end(),
        [](const auto& term){
            return term.key.kind==ConsequenceKind::Resource&&term.key.horizon==1;
        });
    CHECK(future_buffers==1);
    CHECK(std::count_if(decision.norm_ledger.begin(),decision.norm_ledger.end(),
        [](const auto& term){return term.key.kind==ConsequenceKind::Resource;})==2);
}

TEST("norm_conformance",nm066_waiting_clothing_keeps_other_known_choices) {
    auto view=normative_plan_view();
    view.social.enabled=false;
    view.money=0;
    view.food=1;
    view.need.fill(0);
    view.need[4]=.8;
    view.civil.memory.garment_condition=.2;
    view.civil.memory.desired_tier=2;
    view.civil.budget.protected_cash=200;
    add_clothing_shop(view);
    CHECK(civil_options(view).empty());
    Seed random{42,"0.7.0",catalogue_hash(),baseline_catalogue_hash()};
    const auto options=Planner::ideas(view,random);
    CHECK(!options.empty());
    CHECK(std::any_of(options.begin(),options.end(),[](const auto& option){
        return option.method==Method::Leisure||option.method==Method::Rest;
    }));
    CHECK(std::none_of(options.begin(),options.end(),[](const auto& option){
        return option.method==Method::BuyClothes;
    }));
}

TEST("norm_conformance",nm069_norm_focus_keeps_each_accessible_hunger_remedy) {
    auto base=normative_plan_view();
    base.life.enabled=true;
    base.need.fill(0);
    base.need[0]=1;
    base.focus_metric=5;
    const auto norm_key=conformance_key(69,1,1);
    base.norms_view.count=1;
    base.norms_view.considered[0].key=norm_key;
    base.norm_context.effects[0].group_significance=1;
    base.social.norms_view=base.norms_view;
    base.social.norm_context=base.norm_context;
    base.social.norm_options={normative_payload(norm_key,true)};
    Seed random{42,"0.7.0",catalogue_hash(),baseline_catalogue_hash()};

    auto eat=base;
    eat.food=1;
    auto options=Planner::ideas(eat,random);
    CHECK(std::any_of(options.begin(),options.end(),[](const auto& option){
        return option.method==Method::Eat;
    }));

    auto buy=base;
    buy.social.in_conversation=false;
    buy.food=0;
    buy.money=20;
    options=Planner::ideas(buy,random);
    CHECK(std::any_of(options.begin(),options.end(),[](const auto& option){
        return option.method==Method::AcquireFood;
    }));

    auto ask=base;
    ask.food=0;
    ask.money=0;
    ask.perceived_people={2};
    ask.civil.help_candidates={2};
    ask.social.help_people={2};
    ask.social.help_budget=0;
    options=Planner::ideas(ask,random);
    CHECK(std::any_of(options.begin(),options.end(),[](const auto& option){
        return option.method==Method::Social&&option.interaction==Interaction::AskMoney;
    }));
}

TEST("norm_conformance",nm035_group_goal_can_complete_an_unscripted_approval) {
    auto world=conformance_world();
    auto& state=world.edit_for_test();
    auto& actor=state.actors[0];
    auto& other=state.actors[1];
    other.place=actor.place;
    authored_group(actor,35,135,1);
    actor.mind.norm_context.groups.front().members.push_back({2,935'000,0});
    const NormKey key{practice_id(NormPractice::Honesty),35,135,0,1};
    actor.mind.norm_memory.seed_approval(key,{.9,.05,.05},8,
        conformance_source(935'001,NormOrigin::LegacyPrior),0);
    for(auto* participant:{&actor,&other}) {
        participant->mind.social.methods.fill(MethodBelief{});
        participant->mind.social.methods[std::size_t(Interaction::ApprovePractice)]=
            {.9,.9,.9,.9,935'002};
    }
    auto observation=conformance_observation(935'003,key);
    observation.observed_actor=2;
    world.norm_observation_for_test(1,observation);
    CHECK(conformance_run_until(world,[&]{
        return world.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    const auto receiver_integrated=world.state().actors[1].cog.norm.integrated;
    const auto receiver_revision=
        world.state().actors[1].mind.norm_memory.record_revision(key);
    CHECK(receiver_revision==0);
    auto& prepared=world.edit_for_test();
    prepared.actors[0].action={};
    prepared.actors[1].action={};
    prepared.actors[0].mind.social.active_event=0;
    prepared.actors[1].mind.social.active_event=0;
    prepared.actors[1].place=prepared.actors[0].place;
    CHECK(world.conversation_for_test(1,2));
    const auto completed=world.state().social.completed[std::size_t(Interaction::ApprovePractice)];
    world.norm_context_for_test(1);
    CHECK(conformance_run_until(world,[&]{
        const auto& current=world.state();
        return current.social.completed[std::size_t(Interaction::ApprovePractice)]>completed||
               std::any_of(current.actors[0].cog.norm.outbound.begin(),
                           current.actors[0].cog.norm.outbound.end(),[](const auto& payload){
                               return payload.present&&!payload.question&&!payload.explanation&&
                                      payload.evidence.channel==NormChannel::Approval&&
                                       payload.evidence.approval==ApprovalValue::Approve;
                           });
    }));
    CHECK(conformance_run_until(world,[&]{
        return world.state().social.completed[std::size_t(Interaction::ApprovePractice)]>completed;
    }));
    CHECK(conformance_run_until(world,[&]{
        const auto& receiver=world.state().actors[1];
        return receiver.cog.norm.integrated>receiver_integrated&&
               receiver.mind.norm_memory.record_revision(key)>receiver_revision;
    }));
    const auto& receiver=world.state().actors[1];
    const auto learned=receiver.mind.norm_memory.predict(key,world.state().now);
    CHECK(learned.approval_known);
    CHECK(learned.approve>learned.disapprove);
    const auto evidence=receiver.mind.norm_memory.recall_evidence(
        key,NormChannel::Approval,world.state().now);
    CHECK(evidence.has_value());
    CHECK(evidence->source.origin==NormOrigin::Observed);
    CHECK(evidence->source.speaker==1);
    CHECK(evidence->source.revision==1);
}
