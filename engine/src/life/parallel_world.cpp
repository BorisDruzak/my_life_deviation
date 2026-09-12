#include "life/world.hpp"
#include <algorithm>
#include <stdexcept>
namespace life {
bool in_conversation(const Actor& a){return a.conversation.id||(a.action.method==Method::Talk&&a.action.phase==Phase::Running&&a.action.partner);}
std::uint64_t conversation_id(const Actor& a){return a.conversation.id?a.conversation.id:(in_conversation(a)?a.action.id:0);}
Id conversation_partner(const Actor& a){return a.conversation.id?a.conversation.partner:(in_conversation(a)?a.action.partner:0);}
bool conversation_together(const Actor& a,const Actor& b,std::uint64_t parent){
    if(!parent||!a.alive||!b.alive||a.place!=b.place)return false;
    if(a.action.phase==Phase::Travel||b.action.phase==Phase::Travel){
        // The first parallel locomotion profile supports a shared, synchronized leg.
        // Different routes or departure times require a separate rendezvous procedure.
        if(a.action.phase!=Phase::Travel||b.action.phase!=Phase::Travel||
           a.action.segment_start!=b.action.segment_start||a.action.end!=b.action.end||
           a.action.leg+1>=a.action.path.size()||b.action.leg+1>=b.action.path.size()||
           a.action.path[a.action.leg+1]!=b.action.path[b.action.leg+1])return false;
    }
    return in_conversation(a)&&in_conversation(b)&&conversation_id(a)==parent&&conversation_id(b)==parent&&conversation_partner(a)==b.id&&conversation_partner(b)==a.id;
}
bool World::life_start_conversation(Actor& a,const Decision& d){
    if(!state_.life.enabled||!d.partner||d.partner>state_.actors.size()||d.partner==a.id)return false;
    auto& b=state_.actors[d.partner-1];
    physical_until(a,state_.now);physical_until(b,state_.now);
    if(a.conversation.id||b.conversation.id){
        if(!conversation_together(a,b,a.conversation.id))return false;
        if(d.project)a.conversation.project=d.project;
    }else{
        if(a.place!=b.place||!a.alive||!b.alive||a.action.phase==Phase::Travel||b.action.phase==Phase::Travel)return false;
        auto fit=[](const Actor& x){return combine_resources(activity_resources(x.action.method,x.action.phase==Phase::Travel,x.action.phase==Phase::Asleep),activity_resources(Method::Talk));};
        if(!fit(a).allowed||!fit(b).allowed)return false;
        if(capability(a.body,a.mind.cognition,false).gate==0||capability(b.body,b.mind.cognition,b.action.phase==Phase::Asleep).gate==0)return false;
        // Recipient uses OWN motives, own contact history and actual resource capacity.
        const auto* r=b.mind.relation(a.id);const double safety=r?r->trust[3].expectation():.5;
        bool appointment=std::any_of(b.appointments.begin(),b.appointments.end(),[&](const auto& x){return (x.a==a.id||x.b==a.id)&&state_.now>=x.at-900000&&state_.now<x.until;});
        if(safety<=.2||(1-b.social<=.15&&!appointment))return false;
        const auto id=state_.next_id++;const auto end=state_.now+900000;
        capture_self_decision(a,id,d);
        a.conversation={id,d.project,b.id,state_.now,end,0,0};b.conversation={id,0,a.id,state_.now,end,0,0};
        ++state_.life.conversations;
        if((a.action.phase!=Phase::Idle&&a.action.method!=Method::Talk)||(b.action.phase!=Phase::Idle&&b.action.method!=Method::Talk))++state_.life.parallel_started;
        social_known_person(a,b.id,id);social_known_person(b,a.id,id);
        reply_observed(a,b.id,ReplyMessage::Accepted,id);
        ++a.cog.situation_version;++b.cog.situation_version;
        a.cog.review_at=b.cog.review_at=state_.now;
    }
    if(d.project){auto& m=a.mind.projects;auto* p=m.find(d.project);if(p&&p->live()){
        const auto* recipe=m.known(p->procedure);
        if(recipe->steps[p->step].kind==StepKind::Contact)m.observe(p->id,StepOutcome::Success,a.conversation.id,state_.now);
    }}
    // The approach is finished. Conversation occupies its own resource channel.
    if(a.action.method==Method::Talk){a.action=Action{};a.review=state_.now;}
    return true;
}
void World::life_stop_conversation(Actor& actor,bool completed){
    if(!actor.conversation.id)return;
    const auto id=actor.conversation.id;auto& other=state_.actors.at(actor.conversation.partner-1);
    physical_until(actor,state_.now);physical_until(other,state_.now);
    stop_social(actor);
    for(Actor* a:{&actor,&other}){
        if(a->conversation.id!=id)continue;
        const auto c=a->conversation;
        if(c.seconds>0){
            const double quality=c.quality_seconds/c.seconds;
            a->mind.experience_contact(c.partner,id,state_.now,c.seconds,.3*quality,a->place);
            Episode e;e.id=state_.next_id++;e.method=Method::Talk;e.time=state_.now;e.dose=std::min(1.,c.seconds/300.);
            e.outcome[5]=.15*quality;e.observed=1u<<5;e.conscious=true;
            a->mind.learning.observe(e,a->mind.cognition,a->mind.cognition.base[3]);
        }
        if(auto* p=a->mind.projects.find(c.project);p&&p->live()){
            const auto step=a->mind.projects.known(p->procedure)->steps[p->step].kind;
            if(step==StepKind::SpendTime&&c.seconds>=300)a->mind.projects.observe(p->id,StepOutcome::Success,id,state_.now);
            else if(!completed)a->mind.projects.pause(p->id,state_.now,state_.now+60000);
        }
        if(state_.self_enabled&&c.seconds>0){
            OutcomeSignal signal;signal.id=state_.next_id++;signal.root=signal.id;signal.action=id;signal.method=Method::Talk;
            signal.at=state_.now;signal.other=c.partner;signal.completed=completed;signal.interrupted=!completed;
            signal.feedback_observed=true;signal.directness=capability(a->body,a->mind.cognition,false).gate;
            signal.goal_importance=.55;signal.action_feedback=.6;signal.other_decision=.5;
            signal.observed_mask=1u<<5;signal.observed[5]=.15*(c.quality_seconds/c.seconds);
            publish_outcome(*a,signal);
        }
        if(completed)++a->completed[std::size_t(Method::Talk)];
        a->conversation={};++a->mind.version;++a->cog.situation_version;a->review=state_.now;
        if(logger_)logger_({state_.now,a->id,id,Method::Talk,"parallel",completed?"completed":"interrupted",a->place,c.partner});
    }
}
void World::life_conversation_tick(){
    if(!state_.life.enabled)return;
    for(auto& a:state_.actors)if(a.conversation.id){
        const auto& b=state_.actors[a.conversation.partner-1];
        const bool together=conversation_together(a,b,a.conversation.id);
        const auto compatible=[](const Actor& x){return combine_resources(activity_resources(x.action.method,x.action.phase==Phase::Travel,x.action.phase==Phase::Asleep),activity_resources(Method::Talk)).allowed;};
        if(!together||!compatible(a)||!compatible(b)){life_stop_conversation(a,false);continue;}
        // A shared-time step is satisfied by real participation, while the conversation
        // is still available for the next step. Waiting for teardown stranded both plans.
        if(auto* p=a.mind.projects.find(a.conversation.project);p&&p->live()&&a.conversation.seconds>=300){
            const auto* r=a.mind.projects.known(p->procedure);
            if(r&&r->steps[p->step].kind==StepKind::SpendTime){
                a.mind.projects.observe(p->id,StepOutcome::Success,a.conversation.id,state_.now);
                ++a.mind.version;++a.cog.situation_version;a.review=state_.now;
            }
        }
        if(a.conversation.ends<=state_.now)life_stop_conversation(a,true);
    }
}
bool World::conversation_for_test(Id actor,Id partner){Decision d;d.method=Method::Talk;d.partner=partner;d.place=state_.actors.at(actor-1).place;return life_start_conversation(state_.actors.at(actor-1),d);}
}
