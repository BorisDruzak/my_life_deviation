#include "test.hpp"

#include "life/archive.hpp"
#include "life/world.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace life;

namespace {

NormKey runtime_key(std::uint32_t variant=41,Id group=77,std::uint32_t context=19) {
    return {practice_id(NormPractice::Work),group,context,0,variant};
}

NormObservation runtime_observation(std::uint64_t delivery,NormKey key_value=runtime_key()) {
    NormObservation observation;
    observation.key=key_value;
    observation.source={delivery,delivery+10'000,1,2,NormOrigin::Observed,true};
    observation.at=0;
    observation.observed_actor=2;
    observation.quality=1;
    observation.confidence=1;
    observation.reliability=1;
    observation.dose=1;
    observation.value=.8;
    observation.channel=NormChannel::Descriptive;
    observation.applicable=true;
    observation.value_known=true;
    observation.outcome_window_complete=true;
    return observation;
}

World runtime_world(NormMode mode=NormMode::Enabled) {
    auto world=World::generate(42,8);
    NormProfile profile;
    profile.mode=mode;
    world.configure_norm_memory(profile);
    return world;
}

template<class Predicate>
bool run_until(World& world,Predicate predicate,unsigned iterations=12'000) {
    for(unsigned i=0;i<iterations;++i) {
        if(predicate()) return true;
        world.run_ms(25);
    }
    return predicate();
}

std::string self_bytes(const Actor& actor) {
    Writer writer;
    writer(actor.mind.self);
    return writer.data;
}

template<class Value>
std::string runtime_bytes(const Value& value) {
    Writer writer;
    writer(value);
    return writer.data;
}

void author_context(Actor& actor,Id group,std::uint32_t context,Id place,double significance) {
    actor.mind.norm_context.groups.clear();
    actor.mind.norm_context.goals.clear();
    actor.mind.norm_context.questions.clear();
    actor.mind.norm_context.groups.push_back(
        {group,context,place,90'001,{{actor.id,90'002,0}}});
    actor.mind.norm_context.goals.push_back(
        {90'003,90'004,group,significance,1,true});
    ++actor.mind.norm_context.revision;
}

NormSource prior_source(std::uint64_t id) {
    return {id,id,1,0,NormOrigin::LegacyPrior,true};
}

World privacy_social_world(NormMode mode,Id& information_id) {
    auto world=World::generate(42,8);
    world.configure_recovery(false);
    world.edit_for_test().actors[0].mind.social.community.confidentiality=.8;
    NormProfile profile;
    profile.mode=mode;
    world.configure_norm_memory(profile);

    auto& state=world.edit_for_test();
    auto& initiator=state.actors[0];
    auto& receiver=state.actors[1];
    for(auto* actor:{&initiator,&receiver}) {
        actor->place=2;
        actor->action={};
        actor->conversation={};
        actor->mind.social.active_event=0;
        actor->mind.social.inbox.clear();
        actor->mind.social.methods.fill(MethodBelief{});
        actor->mind.social.methods[std::size_t(Interaction::ShareNews)]={1,.4,1,1,state.next_id++};
        actor->mind.known.fill(false);
        actor->mind.known[std::size_t(Method::Social)]=true;
        actor->body.energy=actor->body.water=.95;
        actor->body.sleep=.05;
        actor->body.fatigue=actor->body.damage=actor->body.pain=actor->body.oxygen=0;
        actor->social=.1;
        actor->leisure=.8;
        actor->mind.projects.projects.clear();
        actor->mind.projects.procedures.clear();
        actor->mind.civil.questions.items.clear();
        actor->cog.active=false;
        actor->cog.operation=Operation::None;
        actor->cog.options.clear();
        actor->cog.cursor=0;
        actor->cog.alternatives=4;
        actor->cog.project_id=0;
        actor->affect.causes.clear();
        actor->affect.total.fill(0);
    }
    initiator.mind.social.community.entries.clear();
    initiator.mind.social.community.delivered.clear();
    initiator.mind.social.community.shared.clear();
    Information information;
    information.id=state.next_id++;
    information.kind=NewsKind::Opinion;
    information.subject=8;
    information.importance=.9;
    information.sensitivity=1;
    information.disclosure=Disclosure::Entrusted;
    information.origin=NewsOrigin::Initial;
    information_id=Id(information.id);
    CHECK(initiator.mind.social.community.receive(
        information,0,state.next_id++,state.now));
    CHECK(world.conversation_for_test(initiator.id,receiver.id));
    return world;
}

bool considered_group(const Actor& actor,Id group) {
    const auto& considered=actor.cog.norm.considered;
    return std::any_of(considered.considered.begin(),
                       considered.considered.begin()+considered.count,
                       [&](const auto& prediction){return prediction.key.local_group==group;});
}

std::size_t considered_position(const Actor& actor,Id group) {
    const auto& considered=actor.cog.norm.considered;
    const auto begin=considered.considered.begin();
    const auto end=begin+considered.count;
    return std::size_t(std::find_if(begin,end,[&](const auto& prediction){
        return prediction.key.local_group==group;
    })-begin);
}

} // namespace

TEST("norm_runtime",nm020_gate_zero_preserves_uninterpreted_evidence) {
    auto world=runtime_world();
    auto& actor=world.edit_for_test().actors[0];
    actor.body.oxygen=1;
    const auto revision=actor.mind.norm_memory.revision();
    world.norm_observation_for_test(1,runtime_observation(700'020));
    world.run_ms(500);
    const auto& after=world.state().actors[0];
    CHECK(after.cog.norm.inbox.size()==1);
    CHECK(after.cog.norm.interpreted_count==0);
    CHECK(after.mind.norm_memory.revision()==revision);
}

TEST("norm_runtime",nm021_autonomy_off_queues_without_direct_learning) {
    auto world=runtime_world();
    auto& state=world.edit_for_test();
    state.autonomy=false;
    const auto revision=state.actors[0].mind.norm_memory.revision();
    world.norm_observation_for_test(1,runtime_observation(700'021));
    world.run_seconds(100);
    const auto& actor=world.state().actors[0];
    CHECK(actor.cog.norm.published==1);
    CHECK(actor.cog.norm.inbox.size()==1);
    CHECK(actor.cog.norm.integrated==0);
    CHECK(actor.mind.norm_memory.revision()==revision);
}

TEST("norm_runtime",nm022_interpret_and_integrate_are_two_paid_operations) {
    auto world=runtime_world();
    const auto key=runtime_key();
    const auto& before=world.state().actors[0].cog;
    const auto completed=before.completed_ops;
    const auto interpreted=before.counts[std::size_t(ThoughtKind::NormInterpret)];
    const auto integrated=before.counts[std::size_t(ThoughtKind::NormIntegrate)];
    world.norm_observation_for_test(1,runtime_observation(700'022,key));

    CHECK(run_until(world,[&]{
        return world.state().actors[0].cog.operation==Operation::IntegrateNormEvidence;
    }));
    CHECK(world.state().actors[0].mind.norm_memory.record_revision(key)==0);

    CHECK(run_until(world,[&]{
        return world.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    const auto& after=world.state().actors[0].cog;
    CHECK(after.counts[std::size_t(ThoughtKind::NormInterpret)]==interpreted+1);
    CHECK(after.counts[std::size_t(ThoughtKind::NormIntegrate)]==integrated+1);
    CHECK(after.completed_ops>=completed+2);
    CHECK(after.norm.integrated==1);
}

TEST("norm_runtime",nm024_stale_snapshot_cannot_overwrite_newer_memory) {
    auto world=runtime_world();
    const auto observed=runtime_key(24);
    world.norm_observation_for_test(1,runtime_observation(700'024,observed));
    CHECK(run_until(world,[&]{
        return world.state().actors[0].cog.operation==Operation::InterpretNormObservation;
    }));

    auto& actor=world.edit_for_test().actors[0];
    const auto stale_before=actor.cog.stale_ops;
    const auto unrelated=runtime_key(240);
    actor.mind.norm_memory.seed_principle(unrelated,.6,prior_source(724'000),world.state().now);
    CHECK(run_until(world,[&]{return world.state().actors[0].cog.stale_ops>stale_before;}));
    const auto& after=world.state().actors[0];
    CHECK(after.mind.norm_memory.record_revision(observed)==0);
    CHECK(after.cog.norm.inbox.size()==1);
}

TEST("norm_runtime",nm025_unrecognized_witness_creates_no_clothing_fact) {
    auto world=runtime_world();
    auto& state=world.edit_for_test();
    state.autonomy=false;
    auto& actor=state.actors[0];
    auto& other=state.actors[1];
    other.place=actor.place;
    author_context(actor,77,19,actor.place,1);
    actor.mind.norm_context.groups.front().members.push_back({other.id,90'005,0});
    actor.cog.percepts={{other.id,0,0,80'025,0,1'000'000,0,false,false}};
    const auto published=actor.cog.norm.published;
    world.run_ms(1);
    CHECK(world.state().actors[0].cog.norm.published==published);
    CHECK(world.state().actors[0].cog.norm.inbox.empty());
}

TEST("norm_runtime",nm028_hidden_membership_does_not_enter_personal_context) {
    auto world=runtime_world();
    auto& state=world.edit_for_test();
    state.autonomy=false;
    auto& actor=state.actors[0];
    author_context(actor,88,29,actor.place,1);
    CHECK(!actor.mind.norm_context.member_known(88,2));
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
    CHECK(!world.state().actors[0].mind.norm_context.member_known(88,2));
    CHECK(world.state().actors[0].mind.norm_context.groups.front().members.size()==1);
}

TEST("norm_runtime",nm029_observing_another_does_not_reward_self_model) {
    auto world=runtime_world();
    const auto before=self_bytes(world.state().actors[0]);
    const auto key=runtime_key(29);
    world.norm_observation_for_test(1,runtime_observation(700'029,key));
    CHECK(run_until(world,[&]{
        return world.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    CHECK(self_bytes(world.state().actors[0])==before);
    CHECK(world.state().actors[0].cog.norm.integrated==1);
}

TEST("norm_runtime",nm031_one_record_is_interpreted_and_integrated_once) {
    auto world=runtime_world();
    const auto key=runtime_key(31);
    world.norm_observation_for_test(1,runtime_observation(700'031,key));
    world.run_seconds(100);
    const auto& actor=world.state().actors[0];
    CHECK(actor.cog.norm.published==1);
    CHECK(actor.cog.norm.integrated==1);
    CHECK(actor.cog.norm.inbox.empty());
    CHECK(actor.cog.counts[std::size_t(ThoughtKind::NormInterpret)]==1);
    CHECK(actor.cog.counts[std::size_t(ThoughtKind::NormIntegrate)]==1);
}

TEST("norm_runtime",nm071_context_switch_selects_local_group_and_keeps_old_belief) {
    auto world=runtime_world();
    auto& actor=world.edit_for_test().actors[0];
    const Id place_one=actor.place;
    const Id place_two=place_one==1?2:1;
    actor.mind.norm_context.groups.clear();
    actor.mind.norm_context.goals.clear();
    actor.mind.norm_context.questions.clear();
    actor.mind.norm_context.groups.push_back({71,171,place_one,91'001,{{actor.id,91'002,0}}});
    actor.mind.norm_context.groups.push_back({72,172,place_two,91'003,{{actor.id,91'004,0}}});
    actor.mind.norm_context.goals.push_back({91'005,91'006,71,1,1,true});
    actor.mind.norm_context.goals.push_back({91'007,91'008,72,1,1,true});
    ++actor.mind.norm_context.revision;
    const NormKey first{practice_id(NormPractice::ClothingCondition),71,171,0,0};
    const NormKey second{practice_id(NormPractice::ClothingCondition),72,172,0,0};
    actor.mind.norm_memory.seed_approval(first,{0,1,0},4,prior_source(710'001),0);
    actor.mind.norm_memory.seed_approval(second,{0,1,0},4,prior_source(710'002),0);

    auto evaluations=actor.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&]{return world.state().actors[0].cog.norm.deep_evaluations>evaluations;}));
    const auto& first_context=world.state().actors[0];
    const auto first_local=considered_position(first_context,71);
    const auto first_remote=considered_position(first_context,72);
    CHECK(first_local<first_context.cog.norm.considered.count);
    CHECK(first_remote==first_context.cog.norm.considered.count||first_local<first_remote);

    auto& moved=world.edit_for_test().actors[0];
    moved.place=place_two;
    evaluations=moved.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&]{return world.state().actors[0].cog.norm.deep_evaluations>evaluations;}));
    const auto& second_context=world.state().actors[0];
    const auto second_local=considered_position(second_context,72);
    const auto second_remote=considered_position(second_context,71);
    CHECK(second_local<second_context.cog.norm.considered.count);
    CHECK(second_remote==second_context.cog.norm.considered.count||second_local<second_remote);
    CHECK(world.state().actors[0].mind.norm_memory.record_revision(first)!=0);
    CHECK(world.state().actors[0].mind.norm_memory.record_revision(second)!=0);
}

TEST("norm_runtime",nm073_zero_group_goal_creates_no_conformity_question) {
    auto world=runtime_world();
    auto& actor=world.edit_for_test().actors[0];
    author_context(actor,73,173,actor.place,0);
    actor.mind.civil.garment_condition=0;
    const NormKey key{practice_id(NormPractice::ClothingCondition),73,173,0,0};
    actor.mind.norm_memory.seed_approval(key,{0,1,0},8,prior_source(730'001),0);
    const auto evaluations=actor.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&]{return world.state().actors[0].cog.norm.deep_evaluations>evaluations;}));
    CHECK(considered_group(world.state().actors[0],73));
    CHECK(world.state().actors[0].mind.norm_context.questions.empty());
    CHECK(world.state().actors[0].mind.norm_memory.predict(key,world.state().now).approval_known);
}

TEST("norm_runtime",mode_contract_controls_learning_shadow_and_effects) {
    auto legacy=runtime_world(NormMode::Legacy);
    const auto legacy_revision=legacy.state().actors[0].mind.norm_memory.revision();
    legacy.norm_observation_for_test(1,runtime_observation(701'001));
    legacy.run_seconds(30);
    CHECK(legacy.state().actors[0].cog.norm.inbox.empty());
    CHECK(legacy.state().actors[0].mind.norm_memory.revision()==legacy_revision);

    auto frozen=runtime_world(NormMode::FrozenLearning);
    const auto frozen_key=runtime_key(102);
    const auto main_revision=frozen.state().actors[0].mind.norm_memory.record_revision(frozen_key);
    frozen.norm_observation_for_test(1,runtime_observation(701'002,frozen_key));
    CHECK(run_until(frozen,[&]{return frozen.state().actors[0].cog.norm.shadow_integrated==1;}));
    const auto& frozen_actor=frozen.state().actors[0];
    CHECK(frozen_actor.mind.norm_memory.record_revision(frozen_key)==main_revision);
    CHECK(frozen_actor.cog.norm.frozen_shadow.record_revision(frozen_key)!=0);

    for(auto mode:{NormMode::Shadow,NormMode::NoNormDecisionEffects}) {
        auto world=runtime_world(mode);
        const auto key=runtime_key(103+std::uint32_t(mode));
        world.norm_observation_for_test(1,runtime_observation(701'100+std::uint64_t(mode),key));
        CHECK(run_until(world,[&]{return world.state().actors[0].mind.norm_memory.record_revision(key)!=0;}));
        CHECK(world.state().actors[0].mind.norm_context.questions.empty());
    }
}

TEST("norm_runtime",nm084_inbox_cap_reports_drop_without_free_learning) {
    auto world=runtime_world();
    auto& state=world.edit_for_test();
    state.autonomy=false;
    const auto revision=state.actors[0].mind.norm_memory.revision();
    for(std::uint64_t i=0;i<33;++i)
        world.norm_observation_for_test(1,runtime_observation(784'000+i,runtime_key(std::uint32_t(i+1))));
    const auto& actor=world.state().actors[0];
    CHECK(actor.cog.norm.inbox.size()==32);
    CHECK(actor.cog.norm.dropped==1);
    CHECK(actor.cog.norm.published==32);
    CHECK(actor.cog.norm.integrated==0);
    CHECK(actor.mind.norm_memory.revision()==revision);
}

TEST("norm_runtime",deep_context_evaluation_is_bounded_to_four_records) {
    auto world=runtime_world();
    auto& actor=world.edit_for_test().actors[0];
    author_context(actor,87,187,actor.place,1);
    for(std::uint32_t i=0;i<12;++i) {
        const NormKey key{practice_id(NormPractice::Work),87,187,0,i+1};
        actor.mind.norm_memory.seed_approval(key,{0,1,0},4,prior_source(870'000+i),0);
    }
    const auto before=actor.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&]{return world.state().actors[0].cog.norm.deep_evaluations>before;}));
    const auto& after=world.state().actors[0].cog.norm;
    CHECK(after.considered.count<=4);
    CHECK(after.deep_evaluations-before<=4);
    CHECK(after.candidates_seen>=after.considered.count);
    CHECK(after.candidates_displaced>0);
}

TEST("norm_runtime",prepared_norm_view_expires_on_place_memory_context_or_time_change) {
    auto world=runtime_world();
    auto& actor=world.edit_for_test().actors[0];
    author_context(actor,91,191,actor.place,1);
    const NormKey key{practice_id(NormPractice::Work),91,191,0,1};
    actor.mind.norm_memory.seed_approval(key,{.1,.8,.1},8,prior_source(791'001),0);
    const auto evaluations=actor.cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&]{
        return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
    }));
    CHECK(world.personal_view(1).norms_view.count>0);

    auto moved=world;
    moved.edit_for_test().actors[0].place=actor.place==1?2:1;
    CHECK(moved.personal_view(1).norms_view.count==0);

    auto learned=world;
    learned.edit_for_test().actors[0].mind.norm_memory.seed_principle(
        runtime_key(911),.6,prior_source(791'002),learned.state().now);
    CHECK(learned.personal_view(1).norms_view.count==0);

    auto context_changed=world;
    ++context_changed.edit_for_test().actors[0].mind.norm_context.revision;
    CHECK(context_changed.personal_view(1).norms_view.count==0);

    auto old=world;
    old.edit_for_test().now+=30'001;
    CHECK(old.personal_view(1).norms_view.count==0);
}

TEST("norm_runtime",requested_explanation_survives_compare_deduplicates_and_is_consumed_by_intent) {
    auto world=runtime_world();
    auto& state=world.edit_for_test();
    auto& actor=state.actors[0];
    auto& requester=state.actors[1];
    requester.place=actor.place;
    actor.action={};
    requester.action={};
    CHECK(world.conversation_for_test(1,2));
    author_context(actor,92,192,actor.place,1);
    const NormKey key{practice_id(NormPractice::Help),92,192,0,1};
    actor.mind.norm_memory.apply(runtime_observation(792'001,key),state.now);

    NormPayload question;
    question.present=question.question=true;
    question.key=key;
    question.subject=2;
    InteractionObservation request;
    request.delivery=792'002;
    request.event=792'003;
    request.parent=actor.conversation.id;
    request.other=2;
    request.kind=Interaction::AskPractice;
    request.stage=SocialStage::Completed;
    request.at=state.now;
    request.norm_payload=question;
    world.social_observation_for_test(1,request);
    CHECK(run_until(world,[&]{
        return world.state().actors[0].cog.norm.request_delivery==request.delivery;
    }));

    // Keep paid cognition running while preventing an autonomous social action
    // from consuming the prepared reply before the assertions below.
    world.edit_for_test().actors[0].mind.social.enabled=false;
    auto evaluations=world.state().actors[0].cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&]{
        return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
    }));
    const auto& first=world.state().actors[0].cog.norm.outbound;
    CHECK(first.size()==1);
    CHECK(first.front().present&&first.front().explanation);
    CHECK(first.front().key==key);
    CHECK(first.front().subject==2);

    for(unsigned pass=0;pass<2;++pass) {
        evaluations=world.state().actors[0].cog.norm.deep_evaluations;
        world.norm_context_for_test(1);
        CHECK(run_until(world,[&]{
            return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
        }));
        CHECK(world.state().actors[0].cog.norm.outbound.size()==1);
    }

    const auto reply=world.state().actors[0].cog.norm.outbound.front();
    auto& authored=world.edit_for_test();
    authored.actors[0].mind.social.enabled=true;
    authored.actors[0].mind.social.active_event=0;
    authored.actors[1].mind.social.active_event=0;
    const auto offered=authored.social.events.size();
    world.propose_norm_for_test(1,Interaction::ExplainPractice,2,reply);
    CHECK(world.state().social.events.size()==offered+1);
    CHECK(world.state().actors[0].cog.norm.outbound.empty());
    CHECK(!world.state().actors[0].cog.norm.request.question);
}

TEST("norm_runtime",reported_personal_principle_never_implies_accepted_argument) {
    auto world=runtime_world();
    auto& actor=world.edit_for_test().actors[0];
    const auto key=runtime_key(93);
    actor.mind.norm_memory.seed_principle(key,.8,prior_source(793'001),0);
    const auto before_revision=actor.mind.norm_memory.record_revision(key);
    const auto before_interpreted=actor.cog.norm.interpreted_count;
    const auto before_integrated=actor.cog.norm.integrated;

    NormPayload payload;
    payload.present=payload.explanation=true;
    payload.key=key;
    payload.subject=1;
    payload.evidence=runtime_observation(793'002,key);
    payload.evidence.channel=NormChannel::PersonalPrinciple;
    payload.evidence.value=0;
    payload.evidence.accepted_argument=true;
    payload.evidence.plasticity=1;
    InteractionObservation report;
    report.delivery=793'003;
    report.event=793'004;
    report.parent=793'005;
    report.other=2;
    report.kind=Interaction::ExplainPractice;
    report.stage=SocialStage::Completed;
    report.at=world.state().now;
    report.norm_payload=payload;
    world.social_observation_for_test(1,report);
    CHECK(run_until(world,[&]{
        const auto& current=world.state().actors[0];
        return current.cog.norm.interpreted_count>before_interpreted&&current.cog.norm.inbox.empty();
    }));
    const auto& after=world.state().actors[0];
    CHECK(after.mind.norm_memory.record_revision(key)==before_revision);
    NEAR(after.mind.norm_memory.predict(key,world.state().now).principle_weight,.8,1e-12);
    CHECK(after.cog.norm.integrated==before_integrated);
}

TEST("norm_runtime",default_groups_contain_only_own_contract_and_attended_history_members) {
    const auto world=runtime_world();
    const auto& state=world.state();
    for(const auto& actor:state.actors) {
        const auto* workplace=actor.mind.norm_context.group(1);
        CHECK(workplace!=nullptr);
        std::set<Id> workplace_people;
        for(const auto& member:workplace->members) {
            workplace_people.insert(member.person);
            CHECK(member.source!=0);
        }
        std::set<Id> expected_workplace{actor.id};
        if(actor.employment.supervisor&&actor.employment.supervisor!=actor.id)
            expected_workplace.insert(actor.employment.supervisor);
        CHECK(workplace_people==expected_workplace);

        const auto* local=actor.mind.norm_context.group(2);
        CHECK(local!=nullptr);
        std::set<Id> expected_local{actor.id};
        for(const auto& history:state.history) {
            if(history.place!=local->place||(history.a!=actor.id&&history.b!=actor.id)) continue;
            const Id other=history.a==actor.id?history.b:history.a;
            if(other) expected_local.insert(other);
        }
        std::set<Id> actual_local;
        for(const auto& member:local->members) {
            actual_local.insert(member.person);
            CHECK(member.source!=0);
            if(member.person==actor.id) continue;
            const auto source=std::find_if(state.history.begin(),state.history.end(),[&](const auto& history){
                return history.id==member.source&&history.place==local->place&&
                       (history.a==actor.id||history.b==actor.id)&&
                       (history.a==member.person||history.b==member.person);
            });
            CHECK(source!=state.history.end());
            CHECK(member.at==source->end);
        }
        CHECK(actual_local==expected_local);
    }
}

TEST("norm_runtime",own_contract_revision_adds_work_context_without_erasing_old_context_or_norms) {
    auto world=runtime_world();
    auto& actor=world.edit_for_test().actors[0];
    CHECK(actor.employment.active);
    CHECK(actor.employment.organization!=0);
    CHECK(actor.employment.source!=0);
    const auto old_group=actor.mind.norm_context.employment_group;
    const auto old_source=actor.mind.norm_context.employment_source;
    const auto old_groups=actor.mind.norm_context.groups;
    const auto memory_before=actor.mind.norm_memory;
    const auto new_source=old_source+900'000;
    actor.employment.source=new_source;
    actor.employment.workplace=actor.employment.workplace==7?5:7;
    const auto new_group=Id(1000+actor.employment.organization);
    CHECK(new_group!=old_group);

    world.run_ms(1);
    const auto& after=world.state().actors[0];
    CHECK(after.mind.norm_context.employment_source==new_source);
    CHECK(after.mind.norm_context.employment_group==new_group);
    CHECK(after.mind.norm_context.group(old_group)!=nullptr);
    const auto* current=after.mind.norm_context.group(new_group);
    CHECK(current!=nullptr);
    CHECK(current->place==after.employment.workplace);
    CHECK(current->source==new_source);
    CHECK(after.mind.norm_context.member_known(new_group,after.id));
    CHECK(std::all_of(old_groups.begin(),old_groups.end(),[&](const auto& group){
        return after.mind.norm_context.group(group.id)!=nullptr;
    }));
    CHECK(std::any_of(after.mind.norm_context.goals.begin(),after.mind.norm_context.goals.end(),
        [&](const auto& goal){
            return goal.group==new_group&&goal.goal==new_source&&goal.source==new_source&&goal.active;
        }));
    CHECK(std::none_of(after.mind.norm_context.goals.begin(),after.mind.norm_context.goals.end(),
        [&](const auto& goal){return goal.group==old_group&&goal.active;}));
    CHECK(runtime_bytes(after.mind.norm_memory)==runtime_bytes(memory_before));
}

TEST("norm_runtime",reported_help_approval_can_be_learned_without_inventing_own_violation) {
    auto world=runtime_world();
    auto& actor=world.edit_for_test().actors[0];
    author_context(actor,95,195,actor.place,1);
    const NormKey key{practice_id(NormPractice::Help),95,195,0,1};
    auto report=runtime_observation(795'001,key);
    report.source.origin=NormOrigin::Reported;
    report.source.speaker=2;
    report.channel=NormChannel::Approval;
    report.approval=ApprovalValue::Disapprove;
    world.norm_observation_for_test(1,report);
    CHECK(run_until(world,[&]{
        return world.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    CHECK(world.state().actors[0].mind.norm_memory.predict(key,world.state().now).approval_known);
    const auto evaluations=world.state().actors[0].cog.norm.deep_evaluations;
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&]{
        return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
    }));
    const auto& questions=world.state().actors[0].mind.norm_context.questions;
    CHECK(std::none_of(questions.begin(),questions.end(),[&](const auto& question){
        return question.key==key;
    }));
}

TEST("norm_runtime",categorical_clothing_tension_applies_only_to_disapproved_current_tier) {
    const auto question_for=[](const Actor& actor,const NormKey& key) {
        const auto& questions=actor.mind.norm_context.questions;
        const auto found=std::find_if(questions.begin(),questions.end(),
                                      [&](const auto& question){return question.key==key;});
        return found==questions.end()?static_cast<const NormQuestion*>(nullptr):&*found;
    };
    const auto evaluate=[](World& world) {
        const auto evaluations=world.state().actors[0].cog.norm.deep_evaluations;
        world.norm_context_for_test(1);
        CHECK(run_until(world,[&]{
            return world.state().actors[0].cog.norm.deep_evaluations>evaluations;
        }));
    };

    auto other_disapproved=runtime_world();
    auto& other_disapproved_actor=other_disapproved.edit_for_test().actors[0];
    author_context(other_disapproved_actor,96,196,other_disapproved_actor.place,1);
    other_disapproved_actor.mind.civil.garment_tier=0;
    const NormKey other_tier{practice_id(NormPractice::ClothingTier),96,196,0,2};
    other_disapproved_actor.mind.norm_memory.seed_approval(
        other_tier,{0,1,0},8,prior_source(796'001),0);
    evaluate(other_disapproved);
    CHECK(question_for(other_disapproved.state().actors[0],other_tier)==nullptr);

    auto current_disapproved=runtime_world();
    auto& current_disapproved_actor=current_disapproved.edit_for_test().actors[0];
    author_context(current_disapproved_actor,97,197,current_disapproved_actor.place,1);
    current_disapproved_actor.mind.civil.garment_tier=0;
    const NormKey current_tier{practice_id(NormPractice::ClothingTier),97,197,0,0};
    current_disapproved_actor.mind.norm_memory.seed_approval(
        current_tier,{0,1,0},8,prior_source(797'001),0);
    evaluate(current_disapproved);
    const auto& current_after=current_disapproved.state().actors[0];
    const auto* current_question=question_for(current_after,current_tier);
    CHECK(current_question!=nullptr);
    CHECK(current_question->active);
    CHECK(current_question->tension>=current_after.mind.norm_memory.profile().motive_on);

    auto other_approved=runtime_world();
    auto& other_approved_actor=other_approved.edit_for_test().actors[0];
    author_context(other_approved_actor,98,198,other_approved_actor.place,1);
    other_approved_actor.mind.civil.garment_tier=0;
    const NormKey approved_other_tier{practice_id(NormPractice::ClothingTier),98,198,0,2};
    other_approved_actor.mind.norm_memory.seed_approval(
        approved_other_tier,{1,0,0},8,prior_source(798'001),0);
    evaluate(other_approved);
    CHECK(question_for(other_approved.state().actors[0],approved_other_tier)==nullptr);
}

TEST("norm_runtime",actionable_clothing_question_retrieves_only_its_known_goal_group) {
    const auto prepare=[](World& world,bool actionable_question) {
        auto& state=world.edit_for_test();
        auto& actor=state.actors[0];
        const Id remote_place=actor.place==1?2:1;
        state.ledger.initial_money+=10'000-actor.money;
        actor.money=actor.mind.believed_money=10'000;
        actor.body.energy=actor.body.water=.95;
        actor.body.sleep=.1;
        actor.body.fatigue=actor.body.damage=actor.body.pain=0;
        actor.leisure=1;
        actor.social=0;
        actor.action={};
        actor.conversation={};
        actor.mind.social.active_event=0;
        actor.mind.social.inbox.clear();
        actor.mind.projects.projects.clear();
        for(auto& question:actor.mind.career.questions) {
            question.stage=CareerStage::Deferred;
            question.next_review=state.now+86'400'000;
        }
        actor.cog.inbox.clear();
        actor.cog.outcome_inbox.clear();
        actor.cog.percepts.clear();
        actor.cog.project_id=0;
        actor.cog.focus=6; // Internal Social focus: id == metric + 1.
        actor.cog.focus_metric=5;
        actor.cog.focus_kind=TopicKind::Internal;
        actor.cog.focus_since=state.now;
        actor.cog.rebuild_until=state.now;
        actor.mind.civil.garment_condition=1;
        actor.mind.civil.garment_tier=actor.mind.civil.desired_tier=0;
        const auto garment=std::find_if(state.social.objects.begin(),state.social.objects.end(),
            [&](const auto& object){return object.id==actor.equipment.garment;});
        CHECK(garment!=state.social.objects.end());
        garment->condition=1;
        garment->sku=1;
        garment->wear_per_day=0;
        actor.mind.norm_context.groups.clear();
        actor.mind.norm_context.goals.clear();
        actor.mind.norm_context.questions.clear();
        actor.mind.norm_context.groups.push_back(
            {99,199,remote_place,899'001,{{actor.id,899'002,0}}});
        actor.mind.norm_context.groups.push_back(
            {100,200,remote_place,899'003,{{actor.id,899'004,0}}});
        actor.mind.norm_context.goals.push_back({899'005,899'006,99,1,1,true});
        ++actor.mind.norm_context.revision;

        const NormKey linked{practice_id(NormPractice::ClothingTier),99,199,0,2};
        const NormKey same_group_help{practice_id(NormPractice::Help),99,199,0,1};
        const NormKey unlinked{practice_id(NormPractice::ClothingTier),100,200,0,1};
        const NormKey hidden{practice_id(NormPractice::ClothingTier),101,201,0,2};
        actor.mind.norm_memory.seed_approval(
            linked,{1,0,0},8,prior_source(899'007),0);
        actor.mind.norm_memory.seed_approval(
            same_group_help,{1,0,0},8,prior_source(899'008),0);
        actor.mind.norm_memory.seed_approval(
            unlinked,{1,0,0},8,prior_source(899'009),0);
        actor.mind.norm_memory.seed_approval(
            hidden,{1,0,0},8,prior_source(899'011),0);

        actor.mind.civil.questions.items.erase(
            std::remove_if(actor.mind.civil.questions.items.begin(),
                           actor.mind.civil.questions.items.end(),[](const auto& question) {
                               return question.kind==QuestionKind::Clothing;
                           }),
            actor.mind.civil.questions.items.end());
        if(actionable_question) {
            const Id shop=actor.mind.civil.shops.front().place;
            auto& question=actor.mind.civil.questions.ensure(
                QuestionKind::Clothing,shop,899'010,world.state().now);
            question.pending=false;
            question.actionable=true;
            question.reviewed_basis=question.basis;
        }
        return std::array{linked,same_group_help,unlinked,hidden};
    };
    const auto evaluate=[](World& world) {
        const auto before=world.state().actors[0].cog.norm.deep_evaluations;
        world.norm_context_for_test(1); // Deliberately starts from a non-resource focus.
        CHECK(run_until(world,[&] {
            return world.state().actors[0].cog.norm.deep_evaluations>before;
        }));
        CHECK(world.state().actors[0].cog.norm.captured_focus_metric!=7);
    };
    const auto considered=[](const Actor& actor,const NormKey& key) {
        const auto& view=actor.cog.norm.considered;
        return std::any_of(view.considered.begin(),view.considered.begin()+view.count,
                           [&](const auto& prediction){return prediction.key==key;});
    };

    auto with_question=runtime_world();
    const auto keys=prepare(with_question,true);
    evaluate(with_question);
    const auto& recalled=with_question.state().actors[0];
    CHECK(considered(recalled,keys[0]));
    CHECK(!considered(recalled,keys[1]));
    CHECK(!considered(recalled,keys[2]));
    CHECK(!considered(recalled,keys[3]));

    auto without_question=runtime_world();
    const auto control_keys=prepare(without_question,false);
    evaluate(without_question);
    CHECK(!considered(without_question.state().actors[0],control_keys[0]));
}

TEST("norm_runtime",no_effects_disclosure_risk_does_not_create_personal_violation) {
    Id information=0;
    auto world=privacy_social_world(NormMode::NoNormDecisionEffects,information);
    const auto evaluated=evaluate_social(
        world.personal_view(1).social,Interaction::ShareNews,2,information);
    CHECK(evaluated.known);
    CHECK(evaluated.practical_moral>0);
    NEAR(evaluated.moral,evaluated.practical_moral,1e-15);
    CHECK(evaluated.normative_effect_count==0);

    const auto completed=world.state().social.completed[
        std::size_t(Interaction::ShareNews)];
    world.propose_social_for_test(1,Interaction::ShareNews,2,information);
    CHECK(run_until(world,[&] {
        const auto& state=world.state();
        return state.social.completed[std::size_t(Interaction::ShareNews)]>completed&&
               state.actors[0].mind.social.inbox.empty();
    },20'000));

    const auto& causes=world.state().actors[0].affect.causes;
    CHECK(!causes.empty());
    CHECK(std::none_of(causes.begin(),causes.end(),[](const auto& cause) {
        return cause.target[std::size_t(Emotion::Shame)]>0||
               cause.target[std::size_t(Emotion::Guilt)]>0;
    }));
}

TEST("norm_runtime",paid_share_news_trace_keeps_exact_privacy_provenance) {
    Id information=0;
    auto world=privacy_social_world(NormMode::Enabled,information);
    const NormKey privacy{practice_id(NormPractice::Privacy),0,0,0,1};
    const auto prediction=world.state().actors[0].mind.norm_memory.predict(
        privacy,world.state().now);
    CHECK(prediction.personal_principle_known);
    CHECK(prediction.own_revision>0);

    bool paid_forecast=false;
    std::vector<NormTrace> traces;
    world.set_thought_logger([&](const Thought& thought) {
        if(thought.actor==1&&thought.kind==ThoughtKind::Forecast&&
           thought.method==Method::Social&&
           thought.debug_interaction==Interaction::ShareNews&&
           thought.debug_object==information)
            paid_forecast=true;
    });
    world.set_norm_logger([&](const NormTrace& trace) {
        if(trace.actor==1) traces.push_back(trace);
    });
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&] {
        return paid_forecast&&std::any_of(traces.begin(),traces.end(),
            [&](const auto& trace) {
                return trace.kind=="ledger"&&trace.key==privacy&&
                       trace.consequence_kind==std::uint8_t(ConsequenceKind::PersonalPrinciple);
            });
    },20'000));

    const auto row=std::find_if(traces.begin(),traces.end(),[&](const auto& trace) {
        return trace.kind=="ledger"&&trace.key==privacy&&
               trace.consequence_kind==std::uint8_t(ConsequenceKind::PersonalPrinciple);
    });
    CHECK(row!=traces.end());
    CHECK(row->source_revision==prediction.own_revision);
    CHECK(row->known_source==0);
    CHECK(row->evidence_revision==0);
    CHECK(row->ledger_owner==std::uint8_t(ForecastOwner::NormPrior));
    CHECK(row->knownness==std::uint8_t(ConsequenceKnownness::Known));
    CHECK(row->consequence_target==1);
    CHECK(row->consequence_object==practice_id(NormPractice::Privacy));
    CHECK(row->consequence_horizon==0);
    NEAR(row->probability,1,1e-15);
    NEAR(row->time_hours,0,1e-15);
    CHECK(row->present_value);
    CHECK(row->candidate_method==std::uint32_t(Method::Social));
    CHECK(row->candidate_interaction==std::uint32_t(Interaction::ShareNews));
    CHECK(row->candidate_object==information);
    CHECK(row->value<0);
    CHECK(std::none_of(traces.begin(),traces.end(),[](const auto& trace) {
        return trace.kind=="ledger"&&
               trace.consequence_kind==std::uint8_t(ConsequenceKind::PersonalPrinciple)&&
               (trace.key.practice==0||trace.key.practice==10);
    }));
}
