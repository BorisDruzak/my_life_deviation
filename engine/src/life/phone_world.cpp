#include "life/world.hpp"
#include <algorithm>
#include <limits>
namespace life {
namespace {
const WorldObject* phone_of(const State& s,const Actor& a){
    auto i=std::lower_bound(s.social.objects.begin(),s.social.objects.end(),a.equipment.phone,[](const auto& x,Id id){return x.id<id;});
    return i!=s.social.objects.end()&&i->id==a.equipment.phone&&i->kind==ObjectKind::Phone&&i->owner==a.id&&i->holder==a.id?&*i:nullptr;
}
bool text_fits(const Actor& a){
    if(!a.alive||a.mind.social.active_event||a.action.phase==Phase::Asleep||a.action.method==Method::Sleep)return false;
    auto primary=activity_resources(a.action.method,a.action.phase==Phase::Travel,false);
    const auto text=activity_resources(Method::SendMessage);
    if(primary.exclusive)return false;
    if(a.conversation.id){const auto talk=activity_resources(Method::Talk);for(std::size_t i=0;i<6;++i){const double total=primary.load[i]+talk.load[i]+text.load[i];const bool hard=i==0||i==1||i==4;if(total>(hard?1.:1.5))return false;}return true;}
    return combine_resources(primary,text).allowed;
}
void stop_phone(CivilEquipment& e){e.side_action=0;e.message=0;e.draft=0;e.side_started=0;e.side_end=0;e.reading=false;}
bool valid_norm_text(const NormPayload& payload){return payload.present&&payload.key.practice!=0;}
Interaction norm_text_interaction(const NormPayload& payload){
    if(payload.question)return Interaction::AskPractice;
    if(payload.explanation)return Interaction::ExplainPractice;
    if(payload.evidence.channel==NormChannel::Approval){
        if(payload.evidence.approval==ApprovalValue::Approve)return Interaction::ApprovePractice;
        if(payload.evidence.approval==ApprovalValue::Disapprove)return Interaction::DisapprovePractice;
    }
    return Interaction::ExplainPractice;
}
}
bool World::phone_command(Actor& a,const Decision& d){
    if(d.method!=Method::SendMessage&&d.method!=Method::ReadMessage)return false;
    if(!state_.civil.enabled||a.equipment.side_action||!phone_of(state_,a)||!text_fits(a))return true;
    auto& memory=a.mind.civil;
    if(d.method==Method::SendMessage){
        auto it=std::find_if(memory.drafts.begin(),memory.drafts.end(),[&](const auto& x){return x.id==d.object;});
        if(it==memory.drafts.end()||it->retry_at>state_.now||!memory.number_for(it->person))return true;
        if(it->content.kind==LetterKind::CancelMeeting&&std::find(memory.cancelled_meetings.begin(),memory.cancelled_meetings.end(),it->content.appointment)==memory.cancelled_meetings.end())return true;
        if(it->content.kind==LetterKind::Information&&!a.mind.social.community.recall(it->content.information.id,state_.now))return true;
        if(it->content.kind==LetterKind::Procedure){const auto* known=a.mind.projects.known(it->content.lesson.procedure.id);if(!known||known->mastery<.6)return true;}
        if(it->content.kind==LetterKind::NormPractice&&!valid_norm_text(it->content.norm_payload))return true;
        a.equipment.draft=it->id;a.equipment.reading=false;
    }else{
        auto it=std::find_if(state_.civil.messages.begin(),state_.civil.messages.end(),[&](const auto& x){return x.id==d.object;});
        if(it==state_.civil.messages.end()||it->receiver!=a.id||!it->delivered||it->read||it->failed||std::find(memory.inbox.begin(),memory.inbox.end(),it->id)==memory.inbox.end())return true;
        a.equipment.message=it->id;a.equipment.reading=true;
    }
    a.equipment.side_action=state_.next_id++;a.equipment.side_started=state_.now;a.equipment.side_end=state_.now+(a.equipment.reading?20000:30000);
    capture_self_decision(a,a.equipment.side_action,d);
    civil_record(a,a.equipment.reading?"read_message_started":"send_message_started",d.object,a.equipment.side_action,d.partner,d.object);
    return true;
}
bool World::send_draft_for_test(Id actor,Id draft){auto& a=state_.actors.at(actor-1);const auto before=a.equipment.side_action;Decision d;d.method=Method::SendMessage;d.object=draft;d.place=a.place;d.path={a.place};phone_command(a,d);return !before&&a.equipment.side_action;}
bool World::read_message_for_test(Id actor,std::uint64_t message){if(message>std::numeric_limits<Id>::max())return false;auto& a=state_.actors.at(actor-1);const auto before=a.equipment.side_action;Decision d;d.method=Method::ReadMessage;d.object=Id(message);d.place=a.place;d.path={a.place};phone_command(a,d);return !before&&a.equipment.side_action;}
void World::cancel_local_meeting(Actor& a,std::uint64_t id){
    auto it=std::find_if(a.appointments.begin(),a.appointments.end(),[&](const auto& x){return x.id==id;});
    if(it==a.appointments.end())return;
    const auto target=it->a==a.id?it->b:it->a;
    if(it->a!=a.id&&it->b!=a.id)return;
    a.appointments.erase(it);for(auto& project:a.mind.projects.projects)if(project.live()&&project.appointment==id)a.mind.projects.abandon(project.id,state_.now);
    auto& memory=a.mind.civil;
    if(std::find(memory.cancelled_meetings.begin(),memory.cancelled_meetings.end(),id)==memory.cancelled_meetings.end()){if(memory.cancelled_meetings.size()>=64)memory.cancelled_meetings.erase(memory.cancelled_meetings.begin());memory.cancelled_meetings.push_back(id);}
    LetterContent content;content.kind=LetterKind::CancelMeeting;content.appointment=id;
    memory.compose(target,content,id,state_.now);++a.mind.version;++a.cog.situation_version;
    civil_record(a,"own_meeting_cancelled_notification_pending",id,0,target);a.review=state_.now;a.cog.review_at=state_.now;
}
void World::cancel_meeting_for_test(Id id,std::uint64_t meeting){cancel_local_meeting(state_.actors.at(id-1),meeting);}
void World::civil_departure(Actor& a,const Decision& d){
    if(!state_.civil.enabled||!d.place||d.place==a.place)return;
    std::vector<std::uint64_t> changed;
    for(const auto& m:a.appointments)if(m.place==a.place&&state_.now>=m.at&&state_.now<m.until)changed.push_back(m.id);
    for(auto id:changed)cancel_local_meeting(a,id);
}
void World::phone_tick(){
    auto& s=state_;auto& transport=s.civil;if(!transport.enabled)return;
    // Transport moves only a notification into the inbox. Contents are not known until read.
    for(auto& mail:transport.messages)if(!mail.delivered&&!mail.failed&&mail.deliver_at<=s.now){
        Actor* recipient=nullptr;
        for(auto& person:s.actors){const auto* device=phone_of(s,person);if(device&&device->phone_number==mail.number){recipient=&person;break;}}
        if(!recipient||!recipient->alive){mail.failed=true;++transport.failed_messages;continue;}
        if(recipient->mind.civil.inbox.size()>=32){if(s.now-mail.sent_at>86400000){mail.failed=true;++transport.failed_messages;}else mail.deliver_at=s.now+60000;continue;}
        mail.receiver=recipient->id;mail.delivered=true;recipient->mind.civil.inbox.push_back(mail.id);++transport.deliveries;++recipient->mind.version;++recipient->cog.situation_version;
        recipient->review=recipient->cog.review_at=s.now;
        civil_record(*recipient,"message_delivered_unread",mail.id,0,mail.sender);
    }
    for(auto& a:s.actors){
        auto& equipment=a.equipment;if(!equipment.side_action)continue;
        const bool interrupted=!phone_of(s,a)||!text_fits(a);
        if(!interrupted&&s.now<equipment.side_end)continue;
        const auto action=equipment.side_action;const bool reading=equipment.reading;const auto method=reading?Method::ReadMessage:Method::SendMessage;
        OutcomeSignal outcome;outcome.id=s.next_id++;outcome.root=outcome.id;outcome.action=action;outcome.method=method;outcome.at=s.now;outcome.feedback_observed=true;outcome.goal_importance=.2;outcome.action_feedback=.7;
        outcome.directness=capability(a.body,a.mind.cognition,a.action.phase==Phase::Asleep).gate;
        if(interrupted){
            for(auto& draft:a.mind.civil.drafts)if(draft.id==equipment.draft)draft.retry_at=s.now+60000;
            outcome.interrupted=true;civil_record(a,"phone_action_interrupted",outcome.root,action);stop_phone(equipment);publish_outcome(a,outcome);continue;
        }
        bool completed=false;
        if(!reading){
            auto& memory=a.mind.civil;
            auto it=std::find_if(memory.drafts.begin(),memory.drafts.end(),[&](const auto& x){return x.id==equipment.draft;});
            if(it!=memory.drafts.end()&&memory.number_for(it->person)){
                if(transport.messages.size()>=512){auto old=std::find_if(transport.messages.begin(),transport.messages.end(),[](const auto& e){return e.read||e.failed;});if(old!=transport.messages.end())transport.messages.erase(old);}
                if(transport.messages.size()<512){
                    Envelope envelope;envelope.id=s.next_id++;envelope.source_action=action;envelope.sender=a.id;envelope.number=*memory.number_for(it->person);envelope.from_number=phone_of(s,a)->phone_number;envelope.content=it->content;envelope.sent_at=s.now;envelope.deliver_at=s.now+1000;
                    bool content_available=true;
                    if(envelope.content.kind==LetterKind::Information){auto fact=a.mind.social.community.recall(it->content.information.id,s.now);if(!fact)content_available=false;else envelope.content.information=*fact;}
                    if(envelope.content.kind==LetterKind::Procedure){auto* known=a.mind.projects.known(it->content.lesson.procedure.id);if(!known||known->mastery<.6)content_available=false;else envelope.content.lesson={envelope.id,a.id,*known};}
                    if(envelope.content.kind==LetterKind::NormPractice&&!valid_norm_text(envelope.content.norm_payload))content_available=false;
                    if(content_available){transport.messages.push_back(envelope);civil_record(a,"message_sent_not_yet_delivered",envelope.id,action,it->person);++transport.sends;++memory.sent;memory.drafts.erase(it);completed=true;}
                }
            }
        }else{
            auto it=std::find_if(transport.messages.begin(),transport.messages.end(),[&](const auto& x){return x.id==equipment.message;});
            if(it!=transport.messages.end()&&it->receiver==a.id&&it->delivered&&!it->read&&!it->failed){
                it->read=true;completed=true;++transport.reads;auto& memory=a.mind.civil;++memory.read;
                std::erase(memory.inbox,it->id);if(memory.read_messages.size()>=128)memory.read_messages.erase(memory.read_messages.begin());memory.read_messages.push_back(it->id);
                // Sender address is received from the communication channel, not guessed from an actor ID.
                memory.learn_number(it->sender,it->from_number,it->id,s.now);
                if(it->content.kind==LetterKind::CancelMeeting){
                    auto appointment=std::find_if(a.appointments.begin(),a.appointments.end(),[&](const auto& x){return x.id==it->content.appointment&&((x.a==a.id&&x.b==it->sender)||(x.b==a.id&&x.a==it->sender));});
                    if(appointment!=a.appointments.end()){const auto id=appointment->id;a.appointments.erase(appointment);for(auto& p:a.mind.projects.projects)if(p.live()&&p.appointment==id)a.mind.projects.abandon(p.id,s.now);civil_record(a,"meeting_cancelled_after_reading",it->id,action,it->sender);}
                }else if(it->content.kind==LetterKind::Information||it->content.kind==LetterKind::Procedure||it->content.kind==LetterKind::NormPractice){
                    InteractionObservation observation;observation.delivery=it->id;observation.event=it->id;observation.parent=it->id;observation.outcome_source=it->id;observation.other=it->sender;observation.stage=SocialStage::Completed;observation.at=s.now;observation.dose=.25;
                    observation.kind=it->content.kind==LetterKind::Information?Interaction::ShareNews:it->content.kind==LetterKind::Procedure?Interaction::ExplainProcedure:norm_text_interaction(it->content.norm_payload);
                    observation.information_present=it->content.kind==LetterKind::Information;observation.information=it->content.information;
                    observation.procedure_present=it->content.kind==LetterKind::Procedure;observation.procedure=it->content.lesson;
                    if(it->content.kind==LetterKind::NormPractice)observation.norm_payload=it->content.norm_payload;
                    social_deliver(a,std::move(observation));
                }else{
                    const double gain=std::min(.04,1-a.social);a.social+=gain;a.mind.experience_contact(it->sender,it->id,s.now,20,.2,0);
                    outcome.observed_mask=1u<<5;outcome.observed[5]=gain;outcome.other=it->sender;
                    // A reply is a decision opportunity, never an automatic chain of acknowledgements.
                    if(!it->content.reply&&a.social<.7){LetterContent answer;answer.reply=true;memory.compose(it->sender,answer,it->id,s.now);}
                }
                ++a.mind.version;++a.cog.situation_version;civil_record(a,"message_read",it->id,action,it->sender);
            }
        }
        outcome.completed=completed;outcome.blocked=!completed;
        if(completed)++a.completed[std::size_t(method)];
        if(!completed){outcome.observed_obstacle=1;for(auto& draft:a.mind.civil.drafts)if(draft.id==equipment.draft)draft.retry_at=s.now+60000;}
        stop_phone(equipment);publish_outcome(a,outcome);a.review=a.cog.review_at=s.now;
    }
}
} // namespace life
