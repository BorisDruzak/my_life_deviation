#include "test.hpp"
#include "life/world.hpp"

#include <algorithm>
#include <array>

using namespace life;

namespace {
NormProfile enabled_norms() {
    NormProfile profile;
    profile.mode=NormMode::Enabled;
    return profile;
}

NormPayload stated_rule(std::uint64_t root=9001,double value=.85) {
    NormPayload payload;
    payload.present=true;
    payload.key={practice_id(NormPractice::Help),77,3,0,0};
    payload.subject=1;
    payload.evidence.key=payload.key;
    payload.evidence.source={0,root,4,1,NormOrigin::Observed,false};
    payload.evidence.at=0;
    payload.evidence.observed_actor=1;
    payload.evidence.quality=.9;
    payload.evidence.confidence=.8;
    payload.evidence.reliability=.75;
    payload.evidence.dose=1;
    payload.evidence.value=value;
    payload.evidence.channel=NormChannel::Descriptive;
    payload.evidence.applicable=true;
    payload.evidence.value_known=true;
    payload.evidence.outcome_window_complete=true;
    return payload;
}

InteractionObservation completed_report(std::uint64_t delivery,const NormPayload& payload) {
    InteractionObservation observation;
    observation.delivery=delivery;
    observation.event=delivery;
    observation.parent=delivery;
    observation.other=1;
    observation.kind=Interaction::ExplainPractice;
    observation.stage=SocialStage::Completed;
    observation.at=0;
    observation.norm_payload=payload;
    return observation;
}

const NormRecord& record_for(const Actor& actor,const NormKey& key) {
    const auto& records=actor.mind.norm_memory.records();
    auto found=std::lower_bound(records.begin(),records.end(),key,[](const auto& record,const auto& wanted){return record.key<wanted;});
    CHECK(found!=records.end());
    CHECK(found->key==key);
    return *found;
}

World norm_world() {
    auto world=World::generate(42,8);
    world.configure_recovery();
    world.configure_norm_memory(enabled_norms());
    return world;
}
}

TEST("norm_social", nm032_report_keeps_reported_origin_and_the_speaker) {
    auto world=norm_world();
    auto payload=stated_rule();
    world.social_observation_for_test(2,completed_report(7001,payload));
    world.run_seconds(120);

    const auto& record=record_for(world.state().actors[1],payload.key);
    CHECK(record.sources.size()==1);
    CHECK(record.sources.front().source.origin==NormOrigin::Reported);
    CHECK(record.sources.front().source.speaker==1);
    CHECK(record.sources.front().source.known_root==payload.evidence.source.known_root);
    CHECK(record.sources.front().source.revision==payload.evidence.source.revision);
    CHECK(record.sources.front().source.delivery==7001);
}

TEST("norm_social", nm033_declined_explanation_never_publishes_unheard_content) {
    auto world=norm_world();
    auto& state=world.edit_for_test();
    for(auto& actor:state.actors){actor.place=2;actor.action={};}
    CHECK(world.conversation_for_test(1,2));
    auto& speaker=state.actors[0];
    auto& listener=state.actors[1];
    speaker.mind.social.methods[std::size_t(Interaction::ExplainPractice)]={.9,.5,.9,.9,state.next_id++};
    listener.mind.social.methods[std::size_t(Interaction::ExplainPractice)]={.2,.5,.9,.9,state.next_id++};

    const auto payload=stated_rule();
    const auto before_revision=listener.mind.norm_memory.record_revision(payload.key);
    world.propose_norm_for_test(1,Interaction::ExplainPractice,2,payload);
    world.run_seconds(120);

    CHECK(state.social.declined[std::size_t(Interaction::ExplainPractice)]==1);
    CHECK(listener.mind.norm_memory.record_revision(payload.key)==before_revision);
    CHECK(std::none_of(listener.cog.norm.inbox.begin(),listener.cog.norm.inbox.end(),[&](const auto& observation){return observation.key==payload.key;}));
}

TEST("norm_social", nm036_false_stated_rule_is_not_corrected_from_world_state) {
    auto world=norm_world();
    auto payload=stated_rule(9002,0);
    world.social_observation_for_test(2,completed_report(7002,payload));
    world.run_seconds(120);

    const auto& record=record_for(world.state().actors[1],payload.key);
    CHECK(record.sources.front().negative>0);
    CHECK(record.sources.front().positive==0);
    CHECK(record.sources.front().source.origin==NormOrigin::Reported);
}

TEST("norm_social", nm040_repeated_transport_delivery_has_one_norm_contribution) {
    auto world=norm_world();
    auto observation=completed_report(7003,stated_rule(9003));
    world.social_observation_for_test(2,observation);
    world.social_observation_for_test(2,observation);
    world.run_seconds(120);

    const auto& listener=world.state().actors[1];
    const auto& record=record_for(listener,observation.norm_payload.key);
    CHECK(record.sources.size()==1);
}

TEST("norm_social", nm041_one_speaker_does_not_create_group_consensus) {
    auto world=norm_world();
    const auto groups_before=world.state().actors[1].mind.norm_context.groups;
    world.social_observation_for_test(2,completed_report(7004,stated_rule(9004)));
    world.run_seconds(120);

    const auto& listener=world.state().actors[1];
    const auto& record=record_for(listener,stated_rule(9004).key);
    CHECK(record.sources.size()==1);
    CHECK(record.sources.front().source.speaker==1);
    CHECK(listener.mind.norm_context.groups.size()==groups_before.size());
}

TEST("norm_social", unknown_report_origin_is_pooled_across_distinct_deliveries) {
    auto world=norm_world();
    auto payload=stated_rule(0);
    payload.evidence.source.independently_grounded=false;
    world.social_observation_for_test(2,completed_report(7005,payload));
    world.social_observation_for_test(2,completed_report(7006,payload));
    world.run_seconds(180);

    const auto& record=record_for(world.state().actors[1],payload.key);
    CHECK(record.sources.size()==1);
    CHECK(record.sources.front().source.known_root==0);
    CHECK(!record.sources.front().source.independently_grounded);
    CHECK(record.sources.front().dose==1);
}

TEST("norm_social", completed_reactions_reach_paid_approval_memory_with_event_provenance) {
    constexpr std::array cases{
        std::pair{Interaction::ApprovePractice,ApprovalValue::Approve},
        std::pair{Interaction::DisapprovePractice,ApprovalValue::Disapprove}};
    for(const auto [kind,approval]:cases) {
        auto world=norm_world();
        auto payload=stated_rule();
        payload.subject=2;
        payload.evidence.channel=NormChannel::Approval;
        payload.evidence.approval=approval;
        const auto event=kind==Interaction::ApprovePractice?7101ull:7102ull;
        const auto delivery=kind==Interaction::ApprovePractice?7201ull:7202ull;
        InteractionObservation reaction;
        reaction.delivery=delivery;
        reaction.event=event;
        reaction.parent=7000;
        reaction.other=1;
        reaction.kind=kind;
        reaction.stage=SocialStage::Completed;
        reaction.at=0;
        reaction.norm_payload=payload;

        const auto integrated_before=world.state().actors[1].cog.norm.integrated;
        world.social_observation_for_test(2,reaction);
        CHECK(world.state().actors[1].mind.norm_memory.record_revision(payload.key)==0);
        bool saw_typed_subject=false;
        for(unsigned i=0;i<4800&&world.state().actors[1].cog.norm.integrated==integrated_before;++i) {
            world.run_ms(25);
            const auto& runtime=world.state().actors[1].cog.norm;
            saw_typed_subject|=runtime.input.source.delivery==delivery&&runtime.input.observed_actor==payload.subject;
        }

        const auto& listener=world.state().actors[1];
        CHECK(saw_typed_subject);
        CHECK(listener.cog.norm.integrated==integrated_before+1);
        const auto& record=record_for(listener,payload.key);
        CHECK(record.sources.size()==1);
        CHECK(record.sources.front().channel==NormChannel::Approval);
        CHECK(record.sources.front().approval_value==approval);
        CHECK(record.sources.front().source.origin==NormOrigin::Observed);
        CHECK(record.sources.front().source.known_root==event);
        CHECK(record.sources.front().source.delivery==delivery);
        CHECK(record.sources.front().source.revision==1);
        const auto prediction=listener.mind.norm_memory.predict(payload.key,world.state().now);
        CHECK(prediction.approval_known);
        CHECK((approval==ApprovalValue::Approve?prediction.approve:prediction.disapprove)>1./3);
        CHECK(!prediction.personal_principle_known);
    }
}
