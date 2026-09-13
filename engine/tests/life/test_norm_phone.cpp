#include "test.hpp"
#include "life/world.hpp"

#include <algorithm>

using namespace life;

namespace {
NormProfile enabled_norms() {
    NormProfile profile;
    profile.mode=NormMode::Enabled;
    return profile;
}

NormPayload phone_rule() {
    NormPayload payload;
    payload.present=true;
    payload.key={practice_id(NormPractice::Privacy),31,5,0,0};
    payload.subject=1;
    payload.evidence.key=payload.key;
    payload.evidence.source={0,8100,2,1,NormOrigin::Assumed,false};
    payload.evidence.observed_actor=1;
    payload.evidence.quality=.8;
    payload.evidence.confidence=.7;
    payload.evidence.reliability=.6;
    payload.evidence.dose=1;
    payload.evidence.value=.9;
    payload.evidence.channel=NormChannel::Descriptive;
    payload.evidence.applicable=true;
    payload.evidence.value_known=true;
    payload.evidence.outcome_window_complete=true;
    return payload;
}

World phones(bool learn_number=true) {
    auto world=World::generate(42,8);
    world.configure_recovery();
    world.configure_norm_memory(enabled_norms());
    auto& state=world.edit_for_test();
    state.autonomy=false;
    for(auto& actor:state.actors){actor.mind.civil.drafts.clear();actor.mind.civil.contacts.clear();actor.action={};}
    if(learn_number){
        auto& sender=state.actors[0];
        const auto& receiver=state.actors[1];
        auto phone=std::find_if(state.social.objects.begin(),state.social.objects.end(),[&](const auto& object){return object.id==receiver.equipment.phone;});
        CHECK(phone!=state.social.objects.end());
        sender.mind.civil.learn_number(receiver.id,phone->phone_number,state.next_id++,state.now);
    }
    return world;
}

Id norm_draft(World& world,NormPayload payload=phone_rule()) {
    auto& state=world.edit_for_test();
    LetterContent content;
    content.kind=LetterKind::NormPractice;
    content.norm_payload=payload;
    return state.actors[0].mind.civil.compose(2,content,content.norm_payload.evidence.source.known_root,state.now);
}

const NormRecord& record_for(const Actor& actor,const NormKey& key) {
    const auto& records=actor.mind.norm_memory.records();
    const auto found=std::lower_bound(records.begin(),records.end(),key,[](const auto& record,const auto& wanted){return record.key<wanted;});
    CHECK(found!=records.end());
    CHECK(found->key==key);
    return *found;
}
}

TEST("norm_phone", nm037_delivery_of_unread_text_is_not_norm_knowledge) {
    auto world=phones();
    const auto key=phone_rule().key;
    const auto before=world.state().actors[1].mind.norm_memory.record_revision(key);
    auto draft=norm_draft(world);
    CHECK(draft!=0);
    CHECK(world.send_draft_for_test(1,draft));
    world.run_seconds(32);

    const auto& receiver=world.state().actors[1];
    CHECK(receiver.mind.civil.inbox.size()==1);
    CHECK(receiver.mind.norm_memory.record_revision(key)==before);
    CHECK(receiver.mind.social.inbox.empty());
    CHECK(std::none_of(receiver.cog.norm.inbox.begin(),receiver.cog.norm.inbox.end(),[&](const auto& observation){return observation.key==key;}));
}

TEST("norm_phone", nm038_read_payload_waits_for_paid_social_and_norm_interpretation) {
    auto world=phones();
    const auto key=phone_rule().key;
    const auto before=world.state().actors[1].mind.norm_memory.record_revision(key);
    CHECK(world.send_draft_for_test(1,norm_draft(world)));
    world.run_seconds(32);
    const auto message=world.state().actors[1].mind.civil.inbox.front();
    CHECK(world.read_message_for_test(2,message));
    world.run_seconds(20);

    const auto& receiver=world.state().actors[1];
    CHECK(receiver.mind.norm_memory.record_revision(key)==before);
    CHECK(std::none_of(receiver.cog.norm.inbox.begin(),receiver.cog.norm.inbox.end(),[&](const auto& observation){return observation.key==key;}));
    const auto transported=std::find_if(receiver.mind.social.inbox.begin(),receiver.mind.social.inbox.end(),[&](const auto& observation){return observation.delivery==message;});
    CHECK(transported!=receiver.mind.social.inbox.end());
    CHECK(transported->norm_payload.present);
}

TEST("norm_phone", nm039_unknown_number_cannot_be_derived_from_hidden_actor_id) {
    auto world=phones(false);
    auto& state=world.edit_for_test();
    LetterContent content;
    content.kind=LetterKind::NormPractice;
    content.norm_payload=phone_rule();
    CHECK(state.actors[0].mind.civil.compose(2,content,8100,state.now)==0);
    CHECK(!world.send_draft_for_test(1,2));
    CHECK(state.civil.messages.empty());
}

TEST("norm_phone", approval_evidence_in_an_explanation_remains_a_report) {
    auto world=phones();
    auto payload=phone_rule();
    payload.explanation=true;
    payload.evidence.channel=NormChannel::Approval;
    payload.evidence.approval=ApprovalValue::Disapprove;
    payload.evidence.source.known_root=8123;
    payload.evidence.source.revision=7;
    CHECK(world.send_draft_for_test(1,norm_draft(world,payload)));
    world.run_seconds(32);
    const auto message=world.state().actors[1].mind.civil.inbox.front();
    CHECK(world.read_message_for_test(2,message));
    world.run_seconds(20);
    world.edit_for_test().autonomy=true;
    world.run_seconds(180);

    const auto& record=record_for(world.state().actors[1],payload.key);
    const auto source=std::find_if(record.sources.begin(),record.sources.end(),[](const auto& contribution){
        return contribution.source.known_root==8123;
    });
    CHECK(source!=record.sources.end());
    CHECK(source->channel==NormChannel::Approval);
    CHECK(source->approval_value==ApprovalValue::Disapprove);
    CHECK(source->source.origin==NormOrigin::Reported);
    CHECK(source->source.revision==7);
    CHECK(source->source.delivery==message);
}
