#include "life/social.hpp"
#include "valuation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <limits>
#include <set>

namespace life {
namespace {
std::size_t ix(Interaction k){auto i=std::size_t(k);if(i>=interaction_count)throw std::invalid_argument("interaction enum");return i;}
using Key=std::tuple<Interaction,Id,Id,bool>;
Key key(const ContextExperience& e){return {e.kind,e.person,e.object,e.public_context};}
ContextExperience& experience(SocialMemory& m,Interaction k,Id person,Id object,bool pub){
    const Key wanted{k,person,object,pub};
    auto it=std::lower_bound(m.experiences.begin(),m.experiences.end(),wanted,[](const auto& x,const auto& y){return key(x)<y;});
    if(it==m.experiences.end()||key(*it)!=wanted){ContextExperience x;x.kind=k;x.person=person;x.object=object;x.public_context=pub;it=m.experiences.insert(it,x);}
    return *it;
}
}
const char* interaction_name(Interaction kind){static constexpr const char* n[]={"friendly_touch","romantic_touch","compliment","ask_information","show_item","borrow_item","return_item","claim_item","use_item","share_news","discuss_topic"};return n[ix(kind)];}
const char* interaction_ru(Interaction kind){static constexpr const char* n[]={"дружеское объятие","романтическое объятие","комплимент","спросить сведения","показать вещь","попросить вещь взаймы","вернуть вещь","выдать вещь за свою","использовать предмет","рассказать сведения","обсудить интерес"};return n[ix(kind)];}
const char* social_reason_name(SocialReason r){static constexpr const char* n[]={"none","not_interested","busy","not_understood","boundary","unavailable","withdrawn"};if(unsigned(r)>=std::size(n))throw std::invalid_argument("social reason");return n[unsigned(r)];}
Tick interaction_duration(Interaction k){static constexpr Tick ms[]={5000,8000,5000,6000,6000,5000,5000,6000,60000,60000,120000};return ms[ix(k)];}
bool needs_joint_consent(Interaction kind){return kind!=Interaction::UseItem;}
const ItemMemory* SocialMemory::item(Id id)const{auto it=std::lower_bound(items.begin(),items.end(),id,[](const auto& x,Id k){return x.id<k;});return it!=items.end()&&it->id==id?&*it:nullptr;}
ItemMemory* SocialMemory::item(Id id){return const_cast<ItemMemory*>(std::as_const(*this).item(id));}
const PersonBelief* SocialMemory::person(Id id)const{auto it=std::lower_bound(people.begin(),people.end(),id,[](const auto& x,Id k){return x.id<k;});return it!=people.end()&&it->id==id?&*it:nullptr;}
const ContextExperience* SocialMemory::experience(Interaction k,Id person,Id object,bool pub)const{const Key wanted{k,person,object,pub};auto it=std::lower_bound(experiences.begin(),experiences.end(),wanted,[](const auto& x,const auto& y){return key(x)<y;});return it!=experiences.end()&&key(*it)==wanted?&*it:nullptr;}
void SocialMemory::report_item(const ItemMemory& r,Id speaker,std::uint64_t source,Tick at,bool direct,FactOrigin direct_origin){
    if(!r.id||!source)throw std::invalid_argument("item report requires identity/source");
    auto it=std::lower_bound(items.begin(),items.end(),r.id,[](const auto& x,Id k){return x.id<k;});
    if(it==items.end()||it->id!=r.id){ItemMemory x=r;x.claims.clear();x.origin=direct?direct_origin:FactOrigin::Reported;x.source=source;x.at=at;it=items.insert(it,x);}
    if(std::any_of(it->claims.begin(),it->claims.end(),[&](const auto& x){return x.source==source;}))return;
    ItemClaim claim{speaker,r.owner,r.holder,r.owner_status,r.holder_status,source,direct?direct_origin:FactOrigin::Reported,at};
    it->claims.push_back(claim);
    auto merge=[&](Id value,Truth status,Id& old,Truth& oldstatus){
        if(status==Truth::Unknown)return;
        if(direct||oldstatus==Truth::Unknown){old=value;oldstatus=status;}
        else if(old!=value||oldstatus==Truth::Conflicting)oldstatus=Truth::Conflicting;
    };
    const bool adds_reported_owner=!direct&&it->owner_status==Truth::Unknown&&r.owner_status!=Truth::Unknown;
    // Seeing/using a thing reveals its holder, not a legal ownership relation.
    if(!direct||direct_origin==FactOrigin::Agreement)merge(r.owner,r.owner_status,it->owner,it->owner_status);
    merge(r.holder,r.holder_status,it->holder,it->holder_status);
    if(direct){it->place=r.place;it->kind=r.kind;it->origin=direct_origin;it->source=source;it->at=at;}
    else if(adds_reported_owner){it->origin=FactOrigin::Reported;it->source=source;it->at=at;}
    ++revision;
}
bool SocialMemory::observe(const InteractionObservation& o,const Cognitive& c,double memory){
    ix(o.kind);if(!o.delivery||!o.event)throw std::invalid_argument("social observation identity");
    for(double x:{o.pleasure,o.primary_pleasure,o.approval})require_range(x,-1,1);
    require_range(memory,0,1);require_range(o.dose,0,1);
    auto p=std::lower_bound(processed.begin(),processed.end(),o.delivery);
    if(p!=processed.end()&&*p==o.delivery)return false;
    processed.insert(p,o.delivery);
    auto& e=life::experience(*this,o.kind,o.other,o.object,o.public_context);
    if(o.stage==SocialStage::Offer){++e.incoming;if(e.issued_boundaries)++pressure_received;}
    if(o.stage==SocialStage::Accepted&&o.initiated){++e.accepted;++e.attempts;e.last=o.at;}
    if(o.stage==SocialStage::Declined){
        if(o.initiated){++e.refused;++e.attempts;++refusals_received;
            if(o.reason==SocialReason::NotInterested||o.reason==SocialReason::Boundary)++e.boundaries;
            e.last=o.at;
        }else if(o.reason==SocialReason::NotInterested||o.reason==SocialReason::Boundary)++e.issued_boundaries;
    }
    if(o.pleasure_observed){e.pleasure.observe(o.pleasure,c.learnability,1,o.dose,memory,true);++learned_outcomes;}
    if(o.approval_observed)e.approval.observe(o.approval,c.learnability,1,o.dose,memory,true);
    e.last_event=o.event;
    if(o.item_present){
        const bool report=o.stage==SocialStage::Completed&&(o.kind==Interaction::AskInfo||o.kind==Interaction::ClaimItem);
        const bool agreement=o.stage==SocialStage::Completed&&(o.kind==Interaction::BorrowItem||o.kind==Interaction::ReturnItem);
        report_item(o.item,o.other,o.delivery,o.at,!report,agreement?FactOrigin::Agreement:FactOrigin::Observed);++learned_reports;
    }
    if(o.lesson_present){
        const auto k=ix(o.lesson_kind);auto& belief=methods[k];
        // A single structured text supplies one chunk; two study intervals may
        // cross mastery .60. It does not manufacture experienced Q samples.
        const double alpha=.25*c.learnability*o.lesson.mastery*(.4+.6*c.base[5]);
        belief.mastery=unit(belief.mastery+alpha*(1-belief.mastery));
        belief.expected_pleasure=o.lesson.expected_pleasure;
        belief.expected_acceptance=o.lesson.expected_acceptance;
        belief.confidence=o.lesson.confidence;
        belief.source=o.delivery;++learned_reports;
    }
    if(o.stage==SocialStage::Used){if(auto* item=this->item(o.object))++item->uses;if(goal_object==o.object)goal_used=!o.lesson_present||methods[std::size_t(o.lesson_kind)].mastery>=.60;}
    ++revision;return true;
}
SocialEvaluation evaluate_social(const SocialView& v,Interaction k,Id partner,Id object,bool receiving){
    SocialEvaluation out;const auto i=ix(k);const auto& m=v.memory;const auto& belief=m.methods[i];
    if(!v.enabled||belief.mastery<.6){out.score=-1;return out;}
    out.known=true;out.basis=belief.source;out.pleasure=belief.expected_pleasure;
    out.acceptance=receiving?1:belief.expected_acceptance;
    const bool pub=v.known_audience>0;
    const auto* past=m.experience(k,partner,object,pub);
    if(past){
        if(!receiving)out.acceptance=(2*belief.expected_acceptance+past->accepted-1)/(past->accepted+past->refused);
        // Reconsideration isn't a hard timer: repetition has a finite,
        // decaying expected cost and may still be chosen for an important goal.
        if(!receiving)out.repetition=.10*std::min(6u,past->attempts)*std::exp(-double(std::max<Tick>(0,v.now-past->last))/600000.);
        if(receiving&&past->issued_boundaries)out.repetition=.15*std::min(6u,past->incoming)*m.norms[std::size_t(SocialNorm::Boundary)];
    }
    const auto* person=m.person(partner);
    const auto* item=m.item(object);
    if(k==Interaction::RomanticTouch){
        const auto gender=person?person->gender:Gender::Unknown;
        // Unknown gender isn't proof of matching/mismatching preferences.
        out.pleasure*=gender==Gender::Unknown?.5:m.expected_attraction[unsigned(gender)];
        if(person&&m.gender!=Gender::Unknown&&person->gender==m.gender)
            out.moral+=m.norms[std::size_t(SocialNorm::PersonalRomance)];
        if(pub){out.moral+=.5*m.norms[std::size_t(SocialNorm::Privacy)];out.devaluation=m.public_romance_disapproval;}
        if(m.community.enabled){
            out.moral+=m.community.stranger_romance*(1-(person?person->closeness:0));
            if(person&&person->known_kin)out.moral+=m.community.kin_romance;
            if(person&&person->appearance_known)out.pleasure*=attraction(person->appearance,m.community.preference);
        }
    }
    if(k==Interaction::Compliment)out.status_gain=.25;
    if(k==Interaction::ShareNews){
        if(!m.community.enabled){out.known=false;out.score=-1;return out;}
        if(receiving){out.instrumental=.2*m.community.curiosity;}
        else {const auto* note=m.community.find(object);if(!note||!m.community.recall(object,v.now)){out.known=false;out.score=-1;return out;}
          out.instrumental=.05+.2*note->content.importance;
          const auto reputation=m.community.opinion(partner,v.now);
          const double trust=reputation.honesty?unit(.5+.5**reputation.honesty):.5;
          out.moral+=m.community.disclosure_cost(note->content,trust);
          out.basis=note->first_delivery;
          for(const auto& s:m.community.shared)if(s.claim==object&&s.listener==partner)out.repetition+=.6*std::exp2(-double(std::max<Tick>(0,v.now-s.at))/3600000.);
        }
    }
    if(k==Interaction::DiscussTopic){if(!m.community.enabled){out.known=false;out.score=-1;return out;}out.instrumental=.12*m.community.curiosity;}
    if(m.community.enabled&&(k==Interaction::FriendlyTouch||k==Interaction::RomanticTouch)&&past){out.repetition+=.2*std::min(8u,past->attempts)*std::exp2(-double(std::max<Tick>(0,v.now-past->last))/1800000.);}
    if(k==Interaction::ShowItem||k==Interaction::ClaimItem){
        if(!item){out.known=false;out.score=-1;return out;}
        out.status_gain=item->prestige;
        if(!receiving&&k==Interaction::ClaimItem&&item->owner_status==Truth::Confirmed&&item->owner!=v.self){out.moral+=m.norms[std::size_t(SocialNorm::Honesty)];}
        if(!receiving&&k==Interaction::ClaimItem){out.moral+=.35*m.norms[std::size_t(SocialNorm::Modesty)];if(pub)out.devaluation=m.public_boasting_disapproval;}
    }
    if(k==Interaction::AskInfo){
        if(!receiving&&object&&m.goal_object==object&&(!item||item->owner_status!=Truth::Confirmed))out.instrumental=.8;
        else if(receiving)out.instrumental=.08;
    }
    if(k==Interaction::BorrowItem){
        if(!item){out.known=false;out.score=-1;return out;}
        if(!receiving&&m.goal_object==object&&!m.goal_used)out.instrumental=.9;
        if(receiving){out.instrumental=.22;if(item->holder_status==Truth::Confirmed&&item->holder!=v.self)out.instrumental=-.6;}
        if(receiving&&item->owner_status==Truth::Confirmed&&item->owner!=v.self)out.moral+=m.norms[std::size_t(SocialNorm::Property)];
    }
    if(k==Interaction::ReturnItem){
        if(!item){out.known=false;out.score=-1;return out;}
        out.instrumental=receiving?.7:.25+.40*m.norms[std::size_t(SocialNorm::Promise)];
        if(!receiving&&item->owner_status==Truth::Confirmed&&item->owner==v.self)out.instrumental=-1;
    }
    if(k==Interaction::UseItem){
        if(!item){out.known=false;out.score=-1;return out;}
        out.acceptance=1;out.instrumental=(m.goal_object==object&&!m.goal_used)?.9:.2;
        if(item->holder_status==Truth::Confirmed&&item->holder!=v.self)out.instrumental=-1;
        out.pleasure=item->expected_use;
    }
    // Context-specific experienced pleasure already includes the experienced
    // preference. Blend AFTER the prior is conditioned, never multiply it twice.
    if(past&&past->pleasure.count>0){const auto q=past->pleasure.confidence();out.pleasure=(1-q)*out.pleasure+q*past->pleasure.mean;}
    if(past&&past->approval.count>0){
        const double quality=past->approval.confidence();
        out.status_gain=(1-quality)*out.status_gain+quality*past->approval.mean;
    }
    out.moral=unit(out.moral);
    const double benefit=.35*out.pleasure+.30*v.need_social+(.40*m.status_importance*out.status_gain)+.65*out.instrumental;
    const double refusal_cost=receiving?0:(1-out.acceptance)*(.03+.1*m.rejection_sensitivity);
    const double time=double(interaction_duration(k))/3600000.*.1/24;
    cog04::Ledger ledger;ledger.impulse(100,0,0,out.acceptance*benefit);
    ledger.impulse(101,0,0,-out.moral);ledger.impulse(102,0,0,-out.devaluation*.35);
    ledger.impulse(103,0,0,-out.repetition);ledger.impulse(104,0,0,-refusal_cost);
    ledger.impulse(105,0,0,-time);
    out.score=cog04::squash(ledger.value(24,cog04::cfg::discount_per_hour));return out;
}
std::vector<SocialChoice> social_choices(const SocialView& v){
    std::vector<SocialChoice> out;
    if(!v.enabled||v.occupied)return out;
    auto add=[&](Interaction k,Id who,Id object,double urgency){if(v.memory.methods[ix(k)].mastery>=.6)out.push_back({k,who,object,urgency});};
    if(v.in_conversation&&v.partner){
        if(v.memory.community.enabled){
            for(const auto& memory:v.memory.community.entries){const auto score=v.memory.community.topic_score(memory,v.partner,v.now);
                if(memory.content.id<=std::numeric_limits<Id>::max())add(Interaction::ShareNews,v.partner,Id(memory.content.id),.5+.25*score);}
            add(Interaction::DiscussTopic,v.partner,0,.40+.12*v.memory.community.curiosity);
        }
        add(Interaction::FriendlyTouch,v.partner,0,.42);
        add(Interaction::RomanticTouch,v.partner,0,.40);
        add(Interaction::Compliment,v.partner,0,.35);
        if(v.memory.goal_object&&!v.memory.goal_used){const auto* object=v.memory.item(v.memory.goal_object);
            if(!object||object->owner_status!=Truth::Confirmed)add(Interaction::AskInfo,v.partner,v.memory.goal_object,.98);
            else if(object->owner==v.partner&&object->holder!=v.self)add(Interaction::BorrowItem,v.partner,object->id,.97);
        }
        for(const auto& object:v.memory.items)if(object.holder_status==Truth::Confirmed&&object.holder==v.self){
            if(object.owner_status==Truth::Confirmed&&object.owner==v.partner&&(object.uses>0||v.memory.goal_used))add(Interaction::ReturnItem,v.partner,object.id,.99);
            add(Interaction::ShowItem,v.partner,object.id,.32+.1*object.prestige);
            add(Interaction::ClaimItem,v.partner,object.id,.31+.1*object.prestige);
        }
    }
    {
        for(const auto& object:v.memory.items)if(object.holder_status==Truth::Confirmed&&object.holder==v.self&&v.memory.goal_object==object.id&&!v.memory.goal_used)add(Interaction::UseItem,0,object.id,.96);
    }
    // This is availability/cheap association, not the full valuation for all choices.
    // Known repeated attempts reduce retrieval priority but never physically ban a command.
    for(auto& option:out){if(auto* e=v.memory.experience(option.kind,option.other,option.object,v.known_audience>0))option.priority-=.12*std::min(6u,e->attempts)*std::exp(-double(std::max<Tick>(0,v.now-e->last))/600000.);}
    std::stable_sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.priority!=b.priority?a.priority>b.priority:std::tie(a.kind,a.other,a.object)<std::tie(b.kind,b.other,b.object);});
    if(v.memory.community.enabled){
        std::vector<SocialChoice> diverse;std::set<Interaction> seen;
        for(const auto& x:out)if(seen.insert(x.kind).second){diverse.push_back(x);if(diverse.size()==8)break;}
        for(const auto& x:out){if(diverse.size()==8)break;if(std::none_of(diverse.begin(),diverse.end(),[&](const auto& y){return x.kind==y.kind&&x.other==y.other&&x.object==y.object;}))diverse.push_back(x);}
        out=std::move(diverse);
    }
    if(out.size()>8){out.resize(8);}
    return out;
}
void validate_social_memory(const SocialMemory& m){
    auto check=[](bool ok,const char* why){if(!ok)throw std::runtime_error(std::string("social memory: ")+why);};
    check(unsigned(m.gender)<=unsigned(Gender::Man),"gender");
    for(auto x:m.norms){require_range(x,0,1);}
    for(auto x:m.expected_attraction){require_range(x,0,1);}
    for(auto x:{m.public_romance_disapproval,m.public_boasting_disapproval,m.status_importance,m.rejection_sensitivity})require_range(x,0,1);
    for(const auto& b:m.methods){require_range(b.mastery,0,1);require_range(b.expected_pleasure,-1,1);require_range(b.expected_acceptance,0,1);require_range(b.confidence,0,1);}
    Id previous=0;for(const auto& item:m.items){check(item.id>previous,"item index");previous=item.id;check(unsigned(item.kind)<=unsigned(ObjectKind::Keepsake),"item kind");check(unsigned(item.owner_status)<=3&&unsigned(item.holder_status)<=3,"item status");require_range(item.expected_use,-1,1);require_range(item.prestige,0,1);}
    previous=0;for(const auto& p:m.people){check(p.id>previous,"person index");previous=p.id;check(unsigned(p.gender)<=2,"known gender");}
    check(std::is_sorted(m.processed.begin(),m.processed.end()),"processed index");check(std::adjacent_find(m.processed.begin(),m.processed.end())==m.processed.end(),"duplicate processed");
    check(std::is_sorted(m.experiences.begin(),m.experiences.end(),[](const auto& a,const auto& b){return key(a)<key(b);}),"context index");
    for(const auto& e:m.experiences){ix(e.kind);require_range(e.pleasure.mean,-1,1);require_range(e.approval.mean,-1,1);check(e.accepted>=1&&e.refused>=1,"acceptance counts");}
}
} // namespace life
