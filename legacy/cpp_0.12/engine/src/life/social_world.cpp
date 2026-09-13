#include "life/world.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <set>
#include <utility>

namespace life {
namespace {
WorldObject* object_by_id(State& s,Id id){auto it=std::lower_bound(s.social.objects.begin(),s.social.objects.end(),id,[](const auto& o,Id key){return o.id<key;});return it!=s.social.objects.end()&&it->id==id?&*it:nullptr;}
ItemMemory item_description(const WorldObject& o,Tick at,FactOrigin origin,std::uint64_t source){
    ItemMemory m;m.id=o.id;m.owner=o.owner;m.holder=o.holder;m.place=o.place;m.kind=o.kind;
    m.owner_status=m.holder_status=Truth::Confirmed;m.expected_use=.6;m.prestige=o.prestige;m.at=at;m.origin=origin;m.source=source;return m;
}
bool talking_together(const Actor& a,const Actor& b,std::uint64_t parent){
    return conversation_together(a,b,parent);
}
}
const SocialEvent* World::social_event(std::uint64_t id)const{
    if(!id){return nullptr;}
    const auto& events=state_.social.events;
    auto it=std::lower_bound(events.begin(),events.end(),id,[](const auto& e,auto key){return e.id<key;});return it!=events.end()&&it->id==id?&*it:nullptr;
}
SocialEvent* World::social_event(std::uint64_t id){return const_cast<SocialEvent*>(std::as_const(*this).social_event(id));}
void World::social_known_person(Actor& a,Id id,std::uint64_t source){
    if(!id||id>state_.actors.size())return;
    auto& m=a.mind.social;auto it=std::lower_bound(m.people.begin(),m.people.end(),id,[](const auto& p,Id key){return p.id<key;});
    if(it!=m.people.end()&&it->id==id)return;
    PersonBelief p;p.id=id;p.source=source;p.gender=state_.actors[id-1].mind.social.gender;
    if(state_.community.enabled){p.appearance=state_.actors[id-1].appearance;p.appearance_known=true;const auto* kin=a.mind.relation(id);p.known_kin=kin&&kin->role==1;}
    if(const auto* r=a.mind.relation(id)){p.competence=r->trust[1].expectation();}
    m.people.insert(it,p);++m.revision;
}
void World::populate_social_view(const Actor& a,PersonalView& v)const{
    auto& target=v.social;const auto& m=a.mind.social;
    target.enabled=m.enabled;target.self=a.id;target.place=a.place;target.now=state_.now;
    target.in_conversation=in_conversation(a);
    if(target.in_conversation){target.partner=conversation_partner(a);target.parent=conversation_id(a);}
    target.life_enabled=state_.life.enabled;target.need_desire=v.need[6];target.employed=a.employment.active;target.supervisor=a.employment.supervisor;
    target.occupied=m.active_event!=0;target.need_social=v.need[5];target.need_leisure=v.need[4];
    // Copy only the private content needed by the bounded candidate set. No inbox,
    // processed-ID archive or another actor's preference enters a worker snapshot.
    auto& out=target.memory;out.enabled=m.enabled;out.gender=m.gender;out.methods=m.methods;out.norms=m.norms;
    out.expected_attraction=m.expected_attraction;out.public_romance_disapproval=m.public_romance_disapproval;
    out.public_boasting_disapproval=m.public_boasting_disapproval;out.status_importance=m.status_importance;
    out.rejection_sensitivity=m.rejection_sensitivity;out.goal_object=m.goal_object;out.goal_used=m.goal_used;
    out.revision=m.revision;
    if(m.community.enabled){
        auto& cm=out.community;const auto& own=m.community;
        cm.enabled=true;cm.appearance=own.appearance;cm.preference=own.preference;
        cm.confidentiality=own.confidentiality;cm.curiosity=own.curiosity;cm.ambition=own.ambition;
        cm.stranger_romance=own.stranger_romance;cm.kin_romance=own.kin_romance;
        cm.entries=own.candidates(state_.now,16);
        for(const auto& x:own.shared)if(x.listener==target.partner&&own.find(x.claim))cm.shared.push_back(x);
    }
    std::vector<Id> ids=v.perceived_people;
    if(target.partner&&std::find(ids.begin(),ids.end(),target.partner)==ids.end())ids.push_back(target.partner);
    for(Id id:ids)if(const auto* p=m.person(id)){out.people.push_back(*p);target.perceived.push_back(*p);if(const auto* r=a.mind.relation(id))target.familiarities.emplace_back(id,r->familiarity);}
    std::sort(out.people.begin(),out.people.end(),[](const auto& x,const auto& y){return x.id<y.id;});
    // Audience exists for appraisal only if represented among perceived people.
    for(Id id:v.perceived_people)if(id!=target.partner)++target.known_audience;
    for(const auto& item:m.items)if(item.id==m.goal_object||item.holder==a.id||item.owner==a.id||item.holder==target.partner){auto copy=item;copy.claims.clear();out.items.push_back(std::move(copy));}
    for(const auto& e:m.experiences)if(e.person==0||std::find(ids.begin(),ids.end(),e.person)!=ids.end())out.experiences.push_back(e);
}
void World::social_deliver(Actor& a,InteractionObservation o){
    auto& m=a.mind.social;
    if(!o.delivery)o.delivery=state_.next_id++;
    if(std::binary_search(m.processed.begin(),m.processed.end(),o.delivery))return;
    if(std::any_of(m.inbox.begin(),m.inbox.end(),[&](const auto& x){return x.delivery==o.delivery;}))return;
    publish_social_outcome(a,o);
    m.inbox.push_back(std::move(o));++m.revision;++a.mind.version;++a.cog.situation_version;
    a.cog.review_at=std::min(a.cog.review_at,state_.now);a.review=state_.now;
}
void World::emit_social(const SocialEvent& e,Id actor,const char* result){
    if(!logger_)return;
    const auto& a=state_.actors.at(actor-1);
    EventLog row{state_.now,actor,e.id,Method::Social,"social",result,a.place,actor==e.initiator?e.receiver:e.initiator};
    row.object=e.object;row.parent=e.parent;row.interaction=e.kind;
    row.score=e.response.score;row.moral=e.response.moral;row.risk=e.response.devaluation;
    logger_(row);
}
void World::start_social(Actor& a,const Decision& d){
    if(!a.mind.social.enabled||a.mind.social.active_event||d.partner==0||d.partner>state_.actors.size()||d.partner==a.id)return;
    auto& b=state_.actors[d.partner-1];physical_until(a,state_.now);physical_until(b,state_.now);
    if(!talking_together(a,b,conversation_id(a))||!needs_joint_consent(d.interaction))return;
    if(d.interaction>=Interaction::Introduce&&!state_.life.enabled)return;
    if(d.interaction==Interaction::PartnerIntimacy){
        // This whole joint activity is exclusive, even though its negotiation
        // used the conversation channel. Neither participant can be working.
        const auto available=[](const Actor& x){return x.action.phase==Phase::Idle||(x.action.phase==Phase::Running&&x.action.method==Method::Idle);};
        if(!available(a)||!available(b))return;
        auto* location=static_cast<Place*>(nullptr);for(auto& p:state_.places)if(p.id==a.place)location=&p;
        if(!location||(location->home_owner!=a.id&&location->home_owner!=b.id))return;
    }
    if(d.interaction==Interaction::InviteMeeting&&(d.meeting.at<=state_.now||d.meeting.until<=d.meeting.at||!d.meeting.place))return;
    if(b.mind.social.active_event)return;
    if(d.interaction==Interaction::ShareNews){
        if(!a.mind.social.community.recall(d.object,state_.now))return;
        // One utterance at a time in this shared conversational floor.
        for(const auto& participant:state_.actors)if(participant.place==a.place)if(const auto* x=social_event(participant.mind.social.active_event);x&&x->kind==Interaction::ShareNews&&(x->phase==SocialPhase::Proposed||x->phase==SocialPhase::Accepted))return;
    }
    SocialEvent e;e.id=state_.next_id++;e.parent=conversation_id(a);e.initiator=a.id;e.receiver=b.id;e.object=d.object;
    e.kind=d.interaction;e.meeting=d.meeting;e.project=d.project;e.proposed_at=state_.now;e.ends=state_.now+30000;
    auto av=personal_view(a.id),bv=personal_view(b.id);e.public_a=av.social.known_audience>0;e.public_b=bv.social.known_audience>0;
    capture_self_decision(a,e.id,d);
    state_.social.events.push_back(e);++state_.social.offered[std::size_t(e.kind)];
    a.mind.social.active_event=b.mind.social.active_event=e.id;
    InteractionObservation message;message.event=e.id;message.parent=e.parent;message.other=a.id;message.object=e.object;message.kind=e.kind;
    message.meeting=e.meeting;message.project=e.project;message.stage=SocialStage::Offer;message.at=state_.now;message.public_context=e.public_b;
    if((e.kind==Interaction::ShowItem||e.kind==Interaction::ClaimItem)){
        if(auto* object=object_by_id(state_,e.object);object&&object->holder==a.id){
            // Presentation exposes observable features only. Cultural valuation is
            // a local interpretation of the known kind, not the world's prestige.
            message.item_present=true;auto& cue=message.item;cue.id=object->id;
            cue.kind=object->kind;cue.holder=a.id;cue.holder_status=Truth::Confirmed;
            cue.place=a.place;cue.origin=FactOrigin::Observed;cue.source=e.id;
            cue.prestige=object->kind==ObjectKind::Keepsake?.8:object->kind==ObjectKind::Tool?.4:.2;
        }
    }
    social_deliver(b,message);emit_social(e,a.id,"offered");
}
void World::social_answer(Actor& b,const InteractionObservation& offer,bool accept,const SocialEvaluation& evaluation){
    auto* e=social_event(offer.event);if(!e||e->phase!=SocialPhase::Proposed||e->receiver!=b.id)return;
    auto& a=state_.actors[e->initiator-1];physical_until(a,state_.now);physical_until(b,state_.now);
    if(!talking_together(a,b,e->parent)){stop_social(b);return;}
    e->answered_at=state_.now;e->response=evaluation;
    auto message=[&](Actor& recipient,SocialStage stage,SocialReason reason,bool initiated){
        InteractionObservation o;o.event=e->id;o.outcome_source=e->outcome_source;o.parent=e->parent;o.other=initiated?b.id:a.id;o.object=e->object;o.kind=e->kind;o.meeting=e->meeting;o.project=e->project;o.stage=stage;o.reason=reason;o.initiated=initiated;o.at=state_.now;o.public_context=initiated?e->public_a:e->public_b;social_deliver(recipient,o);
    };
    if(!accept){
        e->phase=SocialPhase::Declined;e->outcome_source=state_.self_enabled?state_.next_id++:0;
        const auto* experience=b.mind.social.experience(e->kind,a.id,e->object,e->public_b);
        e->reason=!evaluation.known?SocialReason::NotUnderstood:(experience&&experience->issued_boundaries?SocialReason::Boundary:SocialReason::NotInterested);
        ++state_.social.declined[std::size_t(e->kind)];
        a.mind.social.active_event=b.mind.social.active_event=0;
        message(a,SocialStage::Declined,e->reason,true);message(b,SocialStage::Declined,e->reason,false);
        emit_social(*e,b.id,"declined");return;
    }
    e->phase=SocialPhase::Accepted;e->ends=state_.now+interaction_duration(e->kind);
    if(state_.life.enabled&&a.conversation.id){
        a.conversation.ends=std::max(a.conversation.ends,e->ends+1000);b.conversation.ends=std::max(b.conversation.ends,e->ends+1000);
    }
    ++state_.social.accepted[std::size_t(e->kind)];
    community_begin_utterance(*e);
    // Internal response is real and separate from what either person predicted.
    // No other actor receives these coefficients.
    auto response=[&](const Actor& who,const Actor& other){double primary=who.social_traits.pleasure[std::size_t(e->kind)];
        if(e->kind==Interaction::RomanticTouch||e->kind==Interaction::PartnerIntimacy){primary*=who.social_traits.attraction[unsigned(other.mind.social.gender)];if(state_.community.enabled)primary*=attraction(other.appearance,who.social_traits.preference);}
        return signed_unit(primary);};
    e->primary_a=response(a,b);e->primary_b=response(b,a);
    auto experienced=[&](const Actor& who,const Actor& other,double primary){std::vector<Feature> f{{400000+Id(e->kind),e->id,1,1},{10000+other.id,e->id,1,.8}};if(e->object)f.push_back({500000+e->object,e->id,1,.9});return signed_unit(primary+.5*who.mind.learning.effects(f,state_.now)[0]);};
    e->pleasure_a=experienced(a,b,e->primary_a);e->pleasure_b=experienced(b,a,e->primary_b);
    if(e->kind==Interaction::Compliment)e->approval_b=.65;
    if(e->kind==Interaction::ShowItem||e->kind==Interaction::ClaimItem){
        // A displayed opinion is feedback, not a universal reputation score.
        if(const auto* item=b.mind.social.item(e->object))e->approval_a=signed_unit(item->prestige*(2*b.social_traits.admiration-1));
        if(e->kind==Interaction::ClaimItem){if(const auto* item=b.mind.social.item(e->object);item&&item->owner_status==Truth::Confirmed&&item->owner!=a.id)e->approval_a=-.7;}
    }
    message(a,SocialStage::Accepted,SocialReason::None,true);
    project(a);project(b);emit_social(*e,b.id,"accepted_scope");
}
void World::stop_social(Actor& actor){
    auto* e=social_event(actor.mind.social.active_event);if(!e)return;
    if(e->phase!=SocialPhase::Proposed&&e->phase!=SocialPhase::Accepted){actor.mind.social.active_event=0;return;}
    auto& a=state_.actors[e->initiator-1];auto& b=state_.actors[e->receiver-1];
    physical_until(a,state_.now);physical_until(b,state_.now);
    const bool experienced=e->phase==SocialPhase::Accepted&&state_.now>e->answered_at;
    const double dose=experienced?unit(double(state_.now-e->answered_at)/double(interaction_duration(e->kind))):0;
    e->phase=SocialPhase::Cancelled;e->outcome_source=state_.self_enabled?state_.next_id++:0;if(e->reason==SocialReason::None)e->reason=SocialReason::Withdrawn;++state_.social.cancelled;
    a.mind.social.active_event=b.mind.social.active_event=0;
    for(Actor* person:{&a,&b}){InteractionObservation o;o.event=e->id;o.outcome_source=e->outcome_source;o.parent=e->parent;o.other=person==&a?b.id:a.id;o.object=e->object;o.kind=e->kind;o.meeting=e->meeting;o.project=e->project;o.stage=SocialStage::Cancelled;o.reason=e->reason;o.at=state_.now;o.initiated=person==&a;o.public_context=person==&a?e->public_a:e->public_b;
        if(experienced){o.pleasure_observed=true;o.pleasure=person==&a?e->pleasure_a:e->pleasure_b;o.primary_pleasure=person==&a?e->primary_a:e->primary_b;o.dose=dose;}
        social_deliver(*person,o);person->exposure.clear();}
    emit_social(*e,actor.id,"cancelled");
}
void World::process_social(){
    // Events are retained for audit, but only active per-actor references are examined.
    std::vector<std::uint64_t> active;
    for(const auto& a:state_.actors)if(a.mind.social.active_event)active.push_back(a.mind.social.active_event);
    std::sort(active.begin(),active.end());active.erase(std::unique(active.begin(),active.end()),active.end());
    for(auto id:active){auto* e=social_event(id);if(!e)throw std::logic_error("missing active social event");
        auto& a=state_.actors[e->initiator-1];auto& b=state_.actors[e->receiver-1];
        if(!talking_together(a,b,e->parent)){stop_social(a);continue;}
        if(e->ends>state_.now)continue;
        if(e->phase==SocialPhase::Proposed){stop_social(a);continue;}
        if(e->phase!=SocialPhase::Accepted)continue;
        physical_until(a,state_.now);physical_until(b,state_.now);
        bool available=true;auto* object=e->object?object_by_id(state_,e->object):nullptr;
        if(e->kind==Interaction::BorrowItem)available=object&&object->owner==b.id&&object->holder==b.id&&object->active_loan==0;
        if(e->kind==Interaction::ReturnItem)available=object&&object->owner==b.id&&object->holder==a.id&&object->active_loan!=0;
        if(e->kind==Interaction::ShowItem||e->kind==Interaction::ClaimItem)available=object&&object->holder==a.id;
        if(!available){e->reason=SocialReason::Unavailable;stop_social(a);continue;}
        if(e->kind==Interaction::BorrowItem){
            object->holder=a.id;object->place=a.place;object->active_loan=e->id;
            state_.social.loans.push_back({e->id,object->id,b.id,a.id,state_.now+3600000,false});
        }
        if(e->kind==Interaction::ReturnItem){
            auto it=std::find_if(state_.social.loans.begin(),state_.social.loans.end(),[&](const auto& l){return l.id==object->active_loan;});
            if(it==state_.social.loans.end())throw std::logic_error("loan missing");
            it->returned=true;object->holder=b.id;object->place=b.place;object->active_loan=0;
        }
        e->phase=SocialPhase::Completed;e->outcome_source=state_.self_enabled?state_.next_id++:0;++state_.social.completed[std::size_t(e->kind)];life_social_completed(*e);
        a.mind.social.active_event=b.mind.social.active_event=0;
        for(Actor* who:{&a,&b}){
            InteractionObservation o;o.event=e->id;o.outcome_source=e->outcome_source;o.parent=e->parent;o.other=who==&a?b.id:a.id;o.object=e->object;o.kind=e->kind;o.meeting=e->meeting;o.project=e->project;o.stage=SocialStage::Completed;o.initiated=who==&a;o.public_context=who==&a?e->public_a:e->public_b;o.at=state_.now;
            o.pleasure=who==&a?e->pleasure_a:e->pleasure_b;o.primary_pleasure=who==&a?e->primary_a:e->primary_b;o.pleasure_observed=true;
            o.approval=who==&a?e->approval_a:e->approval_b;o.approval_observed=(e->kind==Interaction::Compliment||e->kind==Interaction::ShowItem||e->kind==Interaction::ClaimItem);
            if(e->kind==Interaction::BorrowItem||e->kind==Interaction::ReturnItem){o.item_present=true;o.item=item_description(*object,state_.now,FactOrigin::Agreement,e->id);}
            if(e->kind==Interaction::AskInfo&&who==&a){
                // Crucial: the answer comes from B's memory, NOT WorldObject.
                if(const auto* report=b.mind.social.item(e->object)){o.item=*report;o.item_present=true;++state_.social.reports;}
                else {o.item.id=e->object;o.item_present=e->object!=0;}
            }
            if(e->kind==Interaction::ShareNews&&who==&b&&e->information_present){o.information=e->information;o.information_present=true;}
            if(e->kind==Interaction::ClaimItem&&who==&b){
                o.item_present=true;o.item.id=e->object;o.item.owner=a.id;
                o.item.owner_status=Truth::Confirmed;o.item.holder=a.id;
                o.item.holder_status=Truth::Confirmed;o.item.kind=object->kind;
                o.item.place=a.place;o.item.origin=FactOrigin::Reported;
            }
            social_deliver(*who,o);who->exposure.clear();
        }
        community_complete_utterance(*e);
        emit_social(*e,a.id,"completed");
    }
}
double World::social_observe(Actor& a,const InteractionObservation& original){
    auto o=original;
    if(o.stage==SocialStage::Declined&&o.initiated){
        const double inferred_rejection=o.reason==SocialReason::Busy?.1:.25+.5*a.mind.social.rejection_sensitivity;
        o.pleasure=-unit((.1+.35*(1-a.social))*inferred_rejection);o.primary_pleasure=o.pleasure;o.pleasure_observed=true;
    }
    auto& m=a.mind.social;
    const auto cap=capability(a.body,a.mind.cognition,false);
    auto current_learning=a.mind.cognition;current_learning.base=cap.current;
    if(!m.observe(o,current_learning,cap.current[3]))return 0;
    life_social_result(a,o);
    if(o.information_present&&o.stage==SocialStage::Completed&&m.community.enabled){
        auto fact=o.information;
        const bool direct=o.parent==0;
        if(direct)fact.origin=NewsOrigin::Observed;
        if(m.community.receive(fact,direct?0:o.other,o.delivery,state_.now)){
            if(fact.job_terms_present&&!direct)receive_career_information(a,fact.job_terms,fact.id);
            if(logger_)logger_({state_.now,a.id,fact.id,Method::Social,"information",direct?"observed":"reported",a.place,o.other});
        }
    }
    if(o.pleasure_observed){
        // Contextual Q is the refinement, not a second term added to generic Talk Q.
        // W is trained by the existing duration-weighted action windows, not again
        // here on the same social stimulus.
        Appraisal app;app.significance=.6;app.pleasantness=o.pleasure;
        if(o.stage==SocialStage::Declined&&o.initiated){app.loss=-std::min(0.,o.pleasure);app.rejection=app.loss;}
        SocialView view;PersonalView full=personal_view(a.id);view=full.social;
        if(o.stage==SocialStage::Completed){auto e=evaluate_social(view,o.kind,o.other,o.object,!o.initiated);app.violation=e.moral;app.disapproval=e.devaluation;app.responsibility=1;}
        // Only communicated feedback enters the subjective appraisal. No global
        // audience lookup and no automatic physical shame increment.
        if(o.approval_observed)app.disapproval=std::max(app.disapproval,std::max(0.,-o.approval));
        a.affect.set(o.delivery,state_.now+120000,appraisal_targets(app,a.body));
    }
    if(o.stage==SocialStage::Completed&&(o.kind==Interaction::FriendlyTouch||o.kind==Interaction::RomanticTouch)){
        if(auto* relation=a.mind.relation(o.other))relation->trust[3].receive({o.delivery,o.event,unit(.5+.5*o.pleasure),.8});
    }
    if(o.kind==Interaction::ReturnItem&&o.stage==SocialStage::Completed&&!o.initiated){if(auto* relation=a.mind.relation(o.other))relation->trust[2].receive({o.delivery,o.event,1,1});}
    ++a.mind.version;return o.pleasure;
}
bool World::complete_object_use(Actor& a){
    auto* object=object_by_id(state_,a.action.object);
    if(!object||object->holder!=a.id)return false;
    ++object->uses;++state_.social.uses;++state_.social.completed[std::size_t(Interaction::UseItem)];
    InteractionObservation o;o.event=a.action.id;o.object=object->id;o.kind=Interaction::UseItem;o.stage=SocialStage::Used;o.at=state_.now;o.initiated=true;
    o.pleasure=a.social_traits.pleasure[std::size_t(Interaction::UseItem)];o.primary_pleasure=o.pleasure;o.pleasure_observed=true;
    o.item_present=true;o.item.id=object->id;o.item.kind=object->kind;
    o.item.holder=a.id;o.item.holder_status=Truth::Confirmed;o.item.place=a.place;
    o.item.origin=FactOrigin::Observed;o.item.source=o.event;
    if(const auto* known=a.mind.social.item(object->id)){o.item.expected_use=known->expected_use;o.item.prestige=known->prestige;}
    // No owner field comes from WorldObject: holding/reading isn't proof of rights.
    if(object->kind==ObjectKind::Book&&object->readable){o.lesson_present=true;o.lesson=object->lesson;o.lesson_kind=object->lesson_kind;}
    social_deliver(a,o);return true;
}
void World::seed_social(){
    auto& s=state_;
    for(auto& a:s.actors){auto& m=a.mind.social;
        m.gender=s.random.integer("presentation",a.id,0,2)?Gender::Woman:Gender::Man;
        m.status_importance=.1+.7*s.random.uniform("social-value",a.id,0);
        m.rejection_sensitivity=a.cog.rejection_bias;
        for(std::size_t k=0;k<unsigned(Interaction::Introduce);++k){auto& b=m.methods[k];
            b.mastery=k<unsigned(Interaction::Introduce)?.85:0;b.expected_pleasure=a.social_traits.pleasure[k];b.expected_acceptance=.65;b.confidence=.7;b.source=s.next_id++;
        }
        // Differential content. Missing know-how is NOT the person's orientation.
        if(s.random.integer("unknown-romance-procedure",a.id,0,4)==0)m.methods[std::size_t(Interaction::RomanticTouch)].mastery=.4;
        m.expected_attraction[1]=m.expected_attraction[2]=.6;
        a.social_traits.attraction[1]=.1+.8*s.random.uniform("attraction",a.id,1);
        a.social_traits.attraction[2]=.1+.8*s.random.uniform("attraction",a.id,2);
        a.social_traits.admiration=s.random.uniform("status-taste",a.id,0);
    }
    for(auto& a:s.actors)for(const auto& r:a.mind.relations)social_known_person(a,r.person,r.origin);
    for(std::size_t i=1;i<s.actors.size();i+=4){auto& a=s.actors[i];WorldObject o;o.id=1000+a.id;o.owner=o.holder=a.id;o.place=a.home;
        o.kind=i%8==1?ObjectKind::Book:ObjectKind::Keepsake;o.prestige=o.kind==ObjectKind::Book?.2:.85;
        o.lesson={1,.65,.65,.8,s.next_id++};s.social.objects.push_back(o);
        auto m=item_description(o,0,FactOrigin::InitialBelief,s.next_id++);a.mind.social.items.push_back(m);
    }
}
void World::propose_social_for_test(Id id,Interaction kind,Id other,Id object){
    auto& a=state_.actors.at(id-1);Decision d;d.method=Method::Social;d.interaction=kind;d.partner=other;d.place=a.place;d.object=object;start_social(a,d);
}
void World::use_object_for_test(Id id,Id object){auto& a=state_.actors.at(id-1);Decision d;d.method=Method::UseObject;d.object=object;d.place=a.place;d.path={a.place};start(a,d);}
void World::withdraw_social_for_test(Id id){stop_social(state_.actors.at(id-1));}
void World::configure_social_scene(const std::string& scene){
    if(state_.now!=0)throw std::invalid_argument("social scene must be configured at t=0");
    const std::set<std::string> supported{"mixed","friendly","personal-boundaries","romantic","romantic-norm","recipient-refuses","pleasure-mismatch","borrow","borrow-novice","information","status","status-norm"};
    if(!supported.contains(scene))throw std::invalid_argument("unknown social scene");
    auto& s=state_;
    for(auto& a:s.actors){a.mind.social.enabled=scene=="mixed"||a.id<=2;a.mind.social.goal_object=0;a.mind.social.goal_used=false;
        a.place=(scene=="mixed"||a.id<=2)?2:a.home;
    }
    if(scene=="mixed"){
        for(auto& a:s.actors){a.social=.3;a.leisure=.8;}
        return;
    }
    auto& a=s.actors[0];auto& b=s.actors[1];a.social=b.social=.3;a.leisure=b.leisure=.85;
    a.mind.social.gender=b.mind.social.gender=Gender::Man;
    for(Actor* who:{&a,&b}){
        who->mind.social.methods.fill(MethodBelief{});
        for(auto& n:who->mind.social.norms)n=0;
        who->mind.social.public_romance_disapproval=0;who->mind.social.public_boasting_disapproval=0;
        who->mind.social.expected_attraction={0,1,1};who->social_traits.attraction={0,1,1};
        who->mind.social.people.clear();social_known_person(*who,who==&a?b.id:a.id,s.next_id++);
    }
    auto know=[&](Actor& who,Interaction k,double pleasure=.7){who.mind.social.methods[std::size_t(k)]={.9,pleasure,.9,.8,s.next_id++};};
    if(scene=="friendly"){know(a,Interaction::FriendlyTouch);know(b,Interaction::FriendlyTouch);a.mind.social.norms[4]=b.mind.social.norms[4]=1;}
    if(scene=="personal-boundaries"){for(Actor* who:{&a,&b}){know(*who,Interaction::FriendlyTouch,.7);know(*who,Interaction::RomanticTouch,.9);who->mind.social.norms[4]=1;}}
    if(scene=="romantic"||scene=="romantic-norm"||scene=="recipient-refuses"||scene=="pleasure-mismatch"){
        know(a,Interaction::RomanticTouch,.9);know(b,Interaction::RomanticTouch,.9);
        if(scene=="romantic-norm")a.mind.social.norms[4]=1;
        if(scene=="recipient-refuses")b.mind.social.norms[4]=1;
        if(scene=="pleasure-mismatch")a.social_traits.pleasure[std::size_t(Interaction::RomanticTouch)]=-.7;
    }
    if(scene=="borrow"||scene=="borrow-novice"||scene=="information"){
        // This controlled fixture explicitly tests more than one study exposure.
        // Catalogue expansion changes seeded traits, so the fixture's learning
        // rate is fixed rather than relying on an accidental hash sample.
        if(scene=="borrow")a.mind.cognition.learnability=.5;
        WorldObject o;o.id=2000;o.owner=o.holder=2;o.place=2;o.kind=ObjectKind::Book;o.lesson_kind=Interaction::Compliment;o.lesson={1,.5,.7,.8,s.next_id++};s.social.objects.push_back(o);
        auto known=item_description(o,0,FactOrigin::InitialBelief,s.next_id++);
        b.mind.social.items.push_back(known);
        known.owner=known.holder=0;known.owner_status=known.holder_status=Truth::Unknown;a.mind.social.items.push_back(known);a.mind.social.goal_object=o.id;
        for(Actor* who:{&a,&b})for(auto k:{Interaction::AskInfo,Interaction::BorrowItem,Interaction::ReturnItem,Interaction::UseItem})know(*who,k,.15);
        a.mind.social.methods[std::size_t(Interaction::Compliment)].mastery=scene=="borrow-novice"?0:.55;
        if(scene=="information")a.mind.social.methods[std::size_t(Interaction::BorrowItem)].mastery=0;
    }
    if(scene=="status"||scene=="status-norm"){
        WorldObject o;o.id=2000;o.owner=2;o.holder=1;o.place=2;o.kind=ObjectKind::Keepsake;o.prestige=.95;s.social.objects.push_back(o);
        const auto known=item_description(o,0,FactOrigin::InitialBelief,s.next_id++);a.mind.social.items.push_back(known);b.mind.social.items.push_back(known);
        know(a,Interaction::ClaimItem,.3);know(b,Interaction::ClaimItem,.3);a.mind.social.status_importance=1;
        if(scene=="status-norm")a.mind.social.norms[std::size_t(SocialNorm::Honesty)]=1;
    }
    for(auto& who:s.actors)std::sort(who.mind.social.items.begin(),who.mind.social.items.end(),[](const auto& x,const auto& y){return x.id<y.id;});
    const bool old=s.autonomy;s.autonomy=false;command_for_test(1,Method::Talk,2);s.autonomy=old;
    if(a.action.partner!=2)throw std::runtime_error("social fixture failed to start parent conversation");
}
void World::validate_social_world()const{
    const auto& s=state_;auto ensure=[](bool b,const char* why){if(!b)throw std::runtime_error(std::string("social invariant: ")+why);};
    for(const auto& a:s.actors){validate_social_memory(a.mind.social);for(auto value:a.social_traits.pleasure)require_range(value,-1,1);for(auto x:a.social_traits.attraction)require_range(x,0,1);
        if(a.mind.social.active_event){const auto* e=social_event(a.mind.social.active_event);ensure(e&&(e->initiator==a.id||e->receiver==a.id),"active event owner");ensure(e->phase==SocialPhase::Proposed||e->phase==SocialPhase::Accepted,"inactive reference");}
    }
    Id previous=0;for(const auto& object:s.social.objects){ensure(object.id>previous,"object index");previous=object.id;ensure(object.owner>0&&object.owner<=s.actors.size()&&object.holder>0&&object.holder<=s.actors.size(),"object owner/holder");require_range(object.condition,0,1);require_range(object.prestige,0,1);
        if(object.active_loan){auto it=std::find_if(s.social.loans.begin(),s.social.loans.end(),[&](const auto& x){return x.id==object.active_loan;});ensure(it!=s.social.loans.end()&&!it->returned&&it->lender==object.owner&&it->borrower==object.holder,"active loan conservation");}
    }
    std::uint64_t old=0;for(const auto& e:s.social.events){ensure(e.id>old&&e.id<s.next_id,"event id ordering");old=e.id;ensure(e.initiator>0&&e.receiver>0&&e.initiator!=e.receiver&&e.initiator<=s.actors.size()&&e.receiver<=s.actors.size(),"event participants");ensure(e.proposed_at<=s.now,"future proposal");ensure(std::size_t(e.kind)<interaction_count&&unsigned(e.phase)<=unsigned(SocialPhase::Cancelled),"event enum");
        if(e.phase==SocialPhase::Accepted||e.phase==SocialPhase::Completed){ensure(e.answered_at>=e.proposed_at,"answer chronology");ensure(e.ends>=e.answered_at,"effect chronology");}
    }
}
} // namespace life
