#include "life/world.hpp"
#include <algorithm>
namespace life {
namespace {
const WorldObject* object(const State& s,Id id){auto i=std::lower_bound(s.social.objects.begin(),s.social.objects.end(),id,[](const auto& x,Id n){return x.id<n;});return i!=s.social.objects.end()&&i->id==id?&*i:nullptr;}
std::uint16_t known_steps(const Actor& a){
    std::uint16_t mask=1u<<unsigned(StepKind::Finish);
    auto add=[&](StepKind step,bool known){if(known)mask|=std::uint16_t(1u<<unsigned(step));};
    add(StepKind::Visit,a.mind.known[std::size_t(Method::Visit)]);
    add(StepKind::Contact,a.mind.known[std::size_t(Method::Talk)]);
    add(StepKind::Introduce,a.mind.social.methods[std::size_t(Interaction::Introduce)].mastery>=.6);
    add(StepKind::Invite,a.mind.social.methods[std::size_t(Interaction::InviteMeeting)].mastery>=.6);
    add(StepKind::Attend,a.mind.known[std::size_t(Method::Visit)]);
    add(StepKind::SpendTime,a.mind.known[std::size_t(Method::Talk)]);
    add(StepKind::Intimacy,a.mind.social.methods[std::size_t(Interaction::PartnerIntimacy)].mastery>=.6);
    add(StepKind::Work,a.mind.known[std::size_t(Method::Work)]);add(StepKind::Study,a.mind.known[std::size_t(Method::Study)]);return mask;
}
}
bool World::civil_social_transaction(SocialEvent& e){
    if(e.kind<Interaction::AskMoney)return true;
    if(!state_.civil.enabled)return false;
    auto& a=state_.actors.at(e.initiator-1);auto& b=state_.actors.at(e.receiver-1);
    if(e.kind==Interaction::AskMoney){
        const double amount=e.object;const auto budget=civil_budget(b.mind.civil,b.mind.believed_money,b.mind.believed_food,state_.now);
        // Consent was obtained by SocialReply; funds and the giver's own reserved necessities are rechecked now.
        if(amount<=0||amount>36||b.money<amount||b.mind.believed_money-budget.protected_cash<amount)return false;
        b.money-=amount;a.money+=amount;a.mind.believed_money=a.money;b.mind.believed_money=b.money;
        ++a.mind.version;++b.mind.version;++state_.civil.gifts;state_.civil.money_transferred+=amount;
        ++a.mind.civil.gifts_received;a.mind.civil.received_money+=amount;++b.mind.civil.gifts_given;b.mind.civil.given_money+=amount;
        Information fact;fact.kind=NewsKind::Help;fact.other=a.id;fact.valence=.7;fact.importance=.7;community_publish(b,fact);
        civil_record(a,"money_help_received",e.id,e.id,b.id,e.object);civil_record(b,"money_help_given",e.id,e.id,a.id,e.object);
    }else if(e.kind==Interaction::AskPhone){const auto* phone=object(state_,b.equipment.phone);if(!phone||phone->holder!=b.id||phone->owner!=b.id)return false;}
    else if(e.kind==Interaction::ExplainProcedure){
        const auto* p=a.mind.projects.known(e.object);if(!p||p->mastery<.6)return false;
        // Freeze the actually explained version. Sender's practice counters are not copied as receiver experience.
        e.procedure={e.id,a.id,*p};e.procedure_present=true;++state_.civil.lessons;
    }
    return true;
}
void World::civil_social_payload(const SocialEvent& e,Actor& recipient,InteractionObservation& o){
    if(!state_.civil.enabled)return;
    if(e.kind==Interaction::AskPhone&&recipient.id==e.initiator){const auto& b=state_.actors.at(e.receiver-1);const auto* phone=object(state_,b.equipment.phone);if(phone)o.phone_number=phone->phone_number;}
    if(e.kind==Interaction::ExplainProcedure&&recipient.id==e.receiver){o.procedure_present=e.procedure_present;o.procedure=e.procedure;}
}
void World::civil_after_observation(Actor& a,const InteractionObservation& o){
    if(!state_.civil.enabled)return;
    auto& m=a.mind.civil;
    if(o.information_present&&o.information.resource_present){
        const auto& f=o.information.resource;
        if(f.subject!=a.id&&a.mind.social.community.resources.assess(f.subject,state_.now).may_have_spare_money){m.questions.wake(QuestionKind::Money,f.subject,f.root,state_.now);civil_record(a,"resource_hypothesis_may_have_spare_money",f.root,0,f.subject);}
    }
    if(o.stage!=SocialStage::Completed)return;
    if(o.kind==Interaction::AskPhone&&o.phone_number&&o.initiated){
        if(m.learn_number(o.other,o.phone_number,o.event,state_.now))civil_record(a,"phone_number_learned_from_answer",o.event,0,o.other);
    }
    if(o.procedure_present){
        const auto cap=capability(a.body,a.mind.cognition,false);
        // This processing is called only at the end of a paid SocialObserve operation.
        const double understanding=unit(.55+.4*cap.current[3]);
        const bool previously_known=a.mind.projects.known(o.procedure.procedure.id)!=nullptr;
        if(learn_procedure(a.mind.projects,m.lessons,o.procedure,known_steps(a),understanding,state_.now)){
            if(!previously_known){m.questions.wake(QuestionKind::Procedure,o.procedure.procedure.id,o.procedure.root,state_.now);if(a.mind.projects.known(o.procedure.procedure.id))++m.procedures_acquired;}
            civil_record(a,"procedure_explained_not_practised",o.procedure.root,0,o.other,o.procedure.procedure.id);
        }
    }
    if(o.kind==Interaction::AskMoney){if(auto* q=m.questions.find(QuestionKind::Money,o.other)){q->pending=false;q->actionable=false;q->retry_at=state_.now+3*3600000;}}
}
}
