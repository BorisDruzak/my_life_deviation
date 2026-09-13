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
bool life_interaction(Interaction k){return k==Interaction::Introduce||k==Interaction::InviteMeeting||k==Interaction::PartnerIntimacy||k==Interaction::RequestWork||k==Interaction::PraiseWork;}
bool civil_interaction(Interaction k){return k==Interaction::AskMoney||k==Interaction::AskPhone||k==Interaction::ExplainProcedure;}
bool norm_interaction(Interaction k){return k==Interaction::AskPractice||k==Interaction::ExplainPractice||k==Interaction::ApprovePractice||k==Interaction::DisapprovePractice;}
bool legacy_norm_owner(NormMode mode){return mode==NormMode::Legacy||mode==NormMode::Shadow;}
bool valid_norm_payload(Interaction k,const NormPayload& payload){
    if(!payload.present||payload.key.practice==0)return false;
    if(k==Interaction::AskPractice)return payload.question&&!payload.explanation;
    if(payload.evidence.key!=payload.key)return false;
    if(k==Interaction::ExplainPractice)return !payload.question&&payload.explanation;
    if(k==Interaction::ApprovePractice)return !payload.question&&!payload.explanation&&payload.evidence.channel==NormChannel::Approval&&payload.evidence.approval==ApprovalValue::Approve;
    if(k==Interaction::DisapprovePractice)return !payload.question&&!payload.explanation&&payload.evidence.channel==NormChannel::Approval&&payload.evidence.approval==ApprovalValue::Disapprove;
    return false;
}
std::size_t norm_index(const SocialView& v,const NormKey& key){
    for(std::size_t i=0;i<v.norms_view.count;++i)if(v.norms_view.considered[i].key==key)return i;
    return v.norms_view.count;
}
double norm_goal_value(const SocialView& v,const NormKey& key){
    if(!norm_effects_active(v.norms_view.mode))return 0;
    const auto i=norm_index(v,key);return i<v.norms_view.count?v.norm_context.effects[i].group_significance:0;
}
double norm_personal_value(const SocialView& v,const NormKey& key){
    const auto i=norm_index(v,key);if(i>=v.norms_view.count)return 0;
    const auto& prediction=v.norms_view.considered[i];
    return prediction.personal_principle_known?prediction.personal_resistance:0;
}
NormPayload paid_payload(const SocialView& v,NormPractice practice){
    NormPayload payload;if(!norm_effects_active(v.norms_view.mode))return payload;
    for(std::size_t i=0;i<v.norms_view.count;++i)if(v.norms_view.considered[i].key.practice==practice_id(practice)){
        payload.present=true;payload.key=v.norms_view.considered[i].key;payload.evidence.key=payload.key;return payload;
    }
    return payload;
}
struct NormSocialTerms {double group=0,sanction=0;};
const NormPrediction* receiving_practice(const SocialView& v,Interaction k,std::size_t& index){
    index=v.norms_view.count;
    if(!norm_effects_active(v.norms_view.mode)||k!=Interaction::AskMoney)return nullptr;
    if(v.norm_payload.present&&v.norm_payload.key.practice==practice_id(NormPractice::Help)){
        index=norm_index(v,v.norm_payload.key);
        if(index<v.norms_view.count)return &v.norms_view.considered[index];
    }
    for(std::size_t i=0;i<v.norms_view.count;++i)if(v.norms_view.considered[i].key.practice==practice_id(NormPractice::Help)){
        index=i;return &v.norms_view.considered[i];
    }
    return nullptr;
}
NormSocialTerms receiving_norm_terms(const SocialView& v,Interaction k){
    std::size_t index=0;const auto* prediction=receiving_practice(v,k,index);
    if(!prediction)return {};
    const auto& effect=v.norm_context.effects[index];
    for(double x:{effect.group_significance,effect.approval_value,effect.disapproval_value,effect.applicability,effect.exception})require_range(x,0,1);
    const double applies=effect.applicability*(1-effect.exception);
    const auto sanction=prediction->sanction_cost();
    const double seen=effect.audience_known?1:(prediction->seen_known?prediction->seen:0);
    NormSocialTerms result;
    if(prediction->approval_known&&effect.group_significance>0&&seen>0)
        result.group=applies*effect.group_significance*seen*
            (prediction->approve*effect.approval_value-(sanction?0:prediction->disapprove*effect.disapproval_value));
    if(sanction)result.sanction=-applies * *sanction;
    return result;
}
const NormPrediction* personal_principle_prediction(const SocialView& v,NormPractice practice,std::uint32_t variant=0){
    if(!norm_effects_active(v.norms_view.mode))return nullptr;
    for(std::size_t i=0;i<v.norms_view.count;++i){const auto& p=v.norms_view.considered[i];
        if(p.key.practice==practice_id(practice)&&p.key.local_group==0&&p.key.context==0&&p.key.actor_role==0&&p.key.variant==variant&&p.personal_principle_known)return &p;}
    return nullptr;
}
double personal_principle(const SocialView& v,NormPractice practice,std::uint32_t variant=0){
    const auto* prediction=personal_principle_prediction(v,practice,variant);
    return prediction?prediction->personal_resistance:0;
}
enum class NormativeEffectField {Moral,Repetition,Instrumental};
NormativeEffectComponent& effect_component(SocialEvaluation& out,const NormPrediction& prediction){
    for(std::size_t i=0;i<out.normative_effect_count;++i)if(out.normative_effects[i].key==prediction.key){
        auto& component=out.normative_effects[i];
        if(component.source_revision!=prediction.own_revision||component.origin!=prediction.personal_origin)
            throw std::logic_error("one norm key has conflicting paid sources");
        return component;
    }
    if(out.normative_effect_count>=out.normative_effects.size())throw std::logic_error("too many paid normative effects");
    auto& component=out.normative_effects[out.normative_effect_count++];
    component.key=prediction.key;component.source_revision=prediction.own_revision;component.origin=prediction.personal_origin;
    return component;
}
void record_normative_effect(SocialEvaluation& out,const NormPrediction& prediction,double value,NormativeEffectField field){
    if(value==0)return;
    auto& component=effect_component(out,prediction);
    if(field==NormativeEffectField::Moral)component.moral_cost+=value;
    else if(field==NormativeEffectField::Repetition)component.repetition_cost+=value;
    else component.instrumental_value+=value;
}
double normative_effect(SocialEvaluation& out,const SocialView& v,NormPractice practice,double legacy,double factor,NormativeEffectField field,std::uint32_t variant=0){
    if(norm_effects_active(v.norms_view.mode)){
        const auto* prediction=personal_principle_prediction(v,practice,variant);if(!prediction)return 0;
        const double value=factor*prediction->personal_resistance;record_normative_effect(out,*prediction,value,field);return value;
    }
    return legacy_norm_owner(v.norms_view.mode)?factor*legacy:0;
}
void identify_personal_instrumental(SocialEvaluation& out,const SocialView& v,NormPractice practice,double factor){
    const auto* prediction=personal_principle_prediction(v,practice);
    if(!prediction)return;
    out.personal_instrumental=factor*prediction->personal_resistance;
    out.personal_instrumental_key=prediction->key;
    out.personal_instrumental_revision=prediction->own_revision;
    record_normative_effect(out,*prediction,out.personal_instrumental,NormativeEffectField::Instrumental);
}
double normative_value(const SocialView& v,NormPractice practice,double legacy,std::uint32_t variant=0){
    if(norm_effects_active(v.norms_view.mode))return personal_principle(v,practice,variant);
    return legacy_norm_owner(v.norms_view.mode)?legacy:0;
}
double private_scope(const Information& information){
    return information.disclosure==Disclosure::Entrusted?1:information.disclosure==Disclosure::Personal?.45:0;
}
double practical_disclosure_cost(const Information& information,double trust){
    require_range(trust,0,1);
    return .3*information.sensitivity*(1-trust);
}
double disclosure_cost(const SocialView& v,const Information& information,double trust){
    const double privacy=normative_value(v,NormPractice::Privacy,v.memory.community.confidentiality,1);
    return privacy*private_scope(information)+practical_disclosure_cost(information,trust);
}
double topic_score(const SocialView& v,const NewsMemory& memory,Id listener){
    double repeated=0;
    for(const auto& shared:v.memory.community.shared)if(shared.claim==memory.content.id&&shared.listener==listener)repeated+=.5*std::exp2(-double(std::max<Tick>(0,v.now-shared.at))/3600000.);
    return (.15+.3*v.memory.community.curiosity+.4*memory.content.importance)*memory.gist.at(v.now)-disclosure_cost(v,memory.content,.5)-repeated;
}
}
double SocialView::familiarity(Id person) const {
    const auto it=std::find_if(familiarities.begin(),familiarities.end(),[&](const auto& x){return x.first==person;});
    return it==familiarities.end()?0:it->second;
}
void validate_social_evaluation(const SocialEvaluation& evaluation){
    if(evaluation.normative_effect_count>evaluation.normative_effects.size())throw std::invalid_argument("too many serialized normative effects");
    auto finite_nonnegative=[](double value){return std::isfinite(value)&&value>=0;};
    for(std::size_t i=0;i<evaluation.normative_effect_count;++i){const auto& component=evaluation.normative_effects[i];
        if(!component.key.practice||!component.source_revision)throw std::invalid_argument("normative effect needs key and revision");
        if(!finite_nonnegative(component.moral_cost)||!finite_nonnegative(component.repetition_cost)||!finite_nonnegative(component.instrumental_value))throw std::invalid_argument("invalid normative effect value");
    }
    if(!finite_nonnegative(evaluation.practical_moral)||!finite_nonnegative(evaluation.assumed_normative_moral))throw std::invalid_argument("invalid moral component");
    if(evaluation.practical_moral>0&&!evaluation.practical_moral_source_revision)throw std::invalid_argument("practical moral cost needs memory revision");
    if(evaluation.assumed_normative_moral>0&&!evaluation.assumed_normative_source_version)throw std::invalid_argument("assumed normative cost needs source version");
}
const char* interaction_name(Interaction kind){static constexpr const char* n[]={"friendly_touch","romantic_touch","compliment","ask_information","show_item","borrow_item","return_item","claim_item","use_item","share_news","discuss_topic","introduce","invite_meeting","partner_intimacy","request_work","praise_work","ask_money","ask_phone","explain_procedure","ask_practice","explain_practice","approve_practice","disapprove_practice"};return n[ix(kind)];}
const char* interaction_ru(Interaction kind){static constexpr const char* n[]={"дружеское объятие","романтическое объятие","комплимент","спросить сведения","показать вещь","попросить вещь взаймы","вернуть вещь","выдать вещь за свою","использовать предмет","рассказать сведения","обсудить интерес","познакомиться","предложить встречу","совместная интимная близость","потребовать выполнения работы","похвалить за работу","попросить денежную помощь","попросить номер телефона","объяснить последовательность","спросить, что здесь принято","объяснить, что здесь принято","выразить одобрение поступка","выразить неодобрение поступка"};return n[ix(kind)];}
const char* social_reason_name(SocialReason r){static constexpr const char* n[]={"none","not_interested","busy","not_understood","boundary","unavailable","withdrawn"};if(unsigned(r)>=std::size(n))throw std::invalid_argument("social reason");return n[unsigned(r)];}
Tick interaction_duration(Interaction k){static constexpr Tick ms[]={5000,8000,5000,6000,6000,5000,5000,6000,60000,60000,120000,30000,60000,1800000,30000,15000,60000,30000,120000,30000,60000,15000,15000};return ms[ix(k)];}
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
    if(norm_interaction(k)&&!valid_norm_payload(k,v.norm_payload)){out.score=-1;return out;}
    out.known=true;out.basis=belief.source;out.pleasure=belief.expected_pleasure;
    out.acceptance=receiving?1:belief.expected_acceptance;
    const bool pub=v.known_audience>0;
    const auto* past=m.experience(k,partner,object,pub);
    if(past){
        if(!receiving)out.acceptance=(2*belief.expected_acceptance+past->accepted-1)/(past->accepted+past->refused);
        // Reconsideration isn't a hard timer: repetition has a finite,
        // decaying expected cost and may still be chosen for an important goal.
        if(!receiving)out.repetition=.10*std::min(6u,past->attempts)*std::exp(-double(std::max<Tick>(0,v.now-past->last))/600000.);
        if(receiving&&past->issued_boundaries)out.repetition=normative_effect(out,v,NormPractice::Boundary,m.norms[std::size_t(SocialNorm::Boundary)],.15*std::min(6u,past->incoming),NormativeEffectField::Repetition);
    }
    const auto* person=m.person(partner);
    const auto* item=m.item(object);
    if(k==Interaction::RomanticTouch||k==Interaction::PartnerIntimacy){
        const auto gender=person?person->gender:Gender::Unknown;
        // Unknown gender isn't proof of matching/mismatching preferences.
        out.pleasure*=gender==Gender::Unknown?.5:m.expected_attraction[unsigned(gender)];
        if(person&&m.gender!=Gender::Unknown&&person->gender==m.gender)
            out.moral+=normative_effect(out,v,NormPractice::PersonalRomance,m.norms[std::size_t(SocialNorm::PersonalRomance)],1,NormativeEffectField::Moral);
        if(pub){out.moral+=normative_effect(out,v,NormPractice::Privacy,m.norms[std::size_t(SocialNorm::Privacy)],.5,NormativeEffectField::Moral);if(legacy_norm_owner(v.norms_view.mode))out.devaluation=m.public_romance_disapproval;}
        if(m.community.enabled){
            out.moral+=normative_effect(out,v,NormPractice::StrangerRomance,m.community.stranger_romance,1-v.familiarity(partner),NormativeEffectField::Moral);
            if(person&&person->known_kin)out.moral+=normative_effect(out,v,NormPractice::KinRomance,m.community.kin_romance,1,NormativeEffectField::Moral);
            if(person&&person->appearance_known)out.pleasure*=attraction(person->appearance,m.community.preference);
        }
    }
    if(life_interaction(k)&&!v.life_enabled){out.known=false;out.score=-1;return out;}
    if(k==Interaction::PartnerIntimacy){out.instrumental=.5*v.need_desire;if(pub){
        for(std::size_t j=0;j<out.normative_effect_count;++j)out.normative_effects[j].moral_cost=0;
        out.practical_moral=0;out.practical_moral_source_revision=0;
        if(v.norms_view.mode==NormMode::NoNormDecisionEffects){out.moral=0;out.assumed_normative_moral=0;out.assumed_normative_source_version=0;}
        else {out.moral=1;out.assumed_normative_moral=1;out.assumed_normative_source_version=1;}
    }}
    if(k==Interaction::InviteMeeting){
        if(!v.proposal.place||v.proposal.at<=v.now||v.proposal.until<=v.proposal.at){out.known=false;out.score=-1;return out;}
        out.instrumental=.15+.2*v.need_social;
    }
    if(k==Interaction::Introduce)out.instrumental=.1;
    if(k==Interaction::RequestWork){
        if(receiving&&(!v.employed||partner!=v.supervisor)){out.known=false;out.score=-1;return out;}
        const double promise=normative_value(v,NormPractice::Promise,m.norms[std::size_t(SocialNorm::Promise)]);
        out.instrumental=receiving?.25+.4*promise:.35;
        if(receiving)identify_personal_instrumental(out,v,NormPractice::Promise,.4);
    }
    if(civil_interaction(k)){
        if(!v.civil_enabled){out.known=false;out.score=-1;return out;}
        if(k==Interaction::AskMoney){
            if(!object||object>36||(receiving&&v.help_budget<object)){out.score=-1;return out;}
            out.instrumental=receiving?.2+.4*v.familiarity(partner)-double(object)/120.:.65;
            if(receiving){std::size_t norm_i=0;if(const auto* p=receiving_practice(v,k,norm_i);p&&p->personal_principle_known){
                out.instrumental+=p->personal_resistance;record_normative_effect(out,*p,p->personal_resistance,NormativeEffectField::Instrumental);
            }}
            if(!receiving){auto r=m.community.resources.assess(partner,v.now);if(!r.bases.empty())out.basis=r.bases.back();}
        }else if(k==Interaction::AskPhone){
            if(receiving&&!v.has_phone){out.score=-1;return out;}out.instrumental=receiving?.15:.3;
        }else if(k==Interaction::ExplainProcedure){
            if(!receiving&&std::find(v.teachable.begin(),v.teachable.end(),object)==v.teachable.end()){out.known=false;out.score=-1;return out;}
            out.instrumental=receiving?(std::find(v.teachable.begin(),v.teachable.end(),object)==v.teachable.end()?.3:-.35):.12+.15*m.community.curiosity;
        }
        if(past)out.repetition+=std::exp2(-double(std::max<Tick>(0,v.now-past->last))/3600000.)*.7;
    }
    if(norm_interaction(k))out.instrumental=norm_goal_value(v,v.norm_payload.key);
    if(k==Interaction::Compliment)out.status_gain=.25;
    if(k==Interaction::ShareNews){
        if(!m.community.enabled){out.known=false;out.score=-1;return out;}
        if(receiving){out.instrumental=.2*m.community.curiosity;}
        else {const auto* note=m.community.find(object);if(!note||!m.community.recall(object,v.now)){out.known=false;out.score=-1;return out;}
          out.instrumental=.05+.2*note->content.importance;
          const auto reputation=m.community.opinion(partner,v.now);
          const double trust=reputation.honesty?unit(.5+.5**reputation.honesty):.5;
          out.moral+=normative_effect(out,v,NormPractice::Privacy,m.community.confidentiality,private_scope(note->content),NormativeEffectField::Moral,1);
          out.practical_moral=practical_disclosure_cost(note->content,trust);out.moral+=out.practical_moral;
          out.practical_moral_source_revision=m.revision;
          out.basis=note->first_delivery;
          for(const auto& s:m.community.shared)if(s.claim==object&&s.listener==partner)out.repetition+=.6*std::exp2(-double(std::max<Tick>(0,v.now-s.at))/3600000.);
        }
    }
    if(k==Interaction::DiscussTopic){if(!m.community.enabled){out.known=false;out.score=-1;return out;}out.instrumental=.12*m.community.curiosity;}
    if(m.community.enabled&&(k==Interaction::FriendlyTouch||k==Interaction::RomanticTouch)&&past){out.repetition+=.2*std::min(8u,past->attempts)*std::exp2(-double(std::max<Tick>(0,v.now-past->last))/1800000.);}
    if(k==Interaction::ShowItem||k==Interaction::ClaimItem){
        if(!item){out.known=false;out.score=-1;return out;}
        out.status_gain=item->prestige;
        if(!receiving&&k==Interaction::ClaimItem&&item->owner_status==Truth::Confirmed&&item->owner!=v.self){out.moral+=normative_effect(out,v,NormPractice::Honesty,m.norms[std::size_t(SocialNorm::Honesty)],1,NormativeEffectField::Moral);}
        if(!receiving&&k==Interaction::ClaimItem){out.moral+=normative_effect(out,v,NormPractice::Modesty,m.norms[std::size_t(SocialNorm::Modesty)],.35,NormativeEffectField::Moral);if(pub&&legacy_norm_owner(v.norms_view.mode))out.devaluation=m.public_boasting_disapproval;}
    }
    if(k==Interaction::AskInfo){
        if(!receiving&&object&&m.goal_object==object&&(!item||item->owner_status!=Truth::Confirmed))out.instrumental=.8;
        else if(receiving)out.instrumental=.08;
    }
    if(k==Interaction::BorrowItem){
        if(!item){out.known=false;out.score=-1;return out;}
        if(!receiving&&m.goal_object==object&&!m.goal_used)out.instrumental=.9;
        if(receiving){out.instrumental=.22;if(item->holder_status==Truth::Confirmed&&item->holder!=v.self)out.instrumental=-.6;}
        if(receiving&&item->owner_status==Truth::Confirmed&&item->owner!=v.self)out.moral+=normative_effect(out,v,NormPractice::Property,m.norms[std::size_t(SocialNorm::Property)],1,NormativeEffectField::Moral);
    }
    if(k==Interaction::ReturnItem){
        if(!item){out.known=false;out.score=-1;return out;}
        const double promise=normative_value(v,NormPractice::Promise,m.norms[std::size_t(SocialNorm::Promise)]);
        out.instrumental=receiving?.7:.25+.40*promise;
        if(!receiving&&item->owner_status==Truth::Confirmed&&item->owner==v.self)out.instrumental=-1;
        else if(!receiving)identify_personal_instrumental(out,v,NormPractice::Promise,.40);
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
    {const double raw=out.moral;out.moral=unit(raw);if(raw>0){const double scale=out.moral/raw;
        out.practical_moral*=scale;out.assumed_normative_moral*=scale;
        for(std::size_t j=0;j<out.normative_effect_count;++j)out.normative_effects[j].moral_cost*=scale;
    }}
    const double benefit=.35*out.pleasure+.30*v.need_social+(.40*m.status_importance*out.status_gain)+.65*out.instrumental;
    const double refusal_cost=receiving?0:(1-out.acceptance)*(.03+.1*m.rejection_sensitivity);
    const double time=double(interaction_duration(k))/3600000.*.1/24;
    const auto norm_terms=receiving?receiving_norm_terms(v,k):NormSocialTerms{};
    cog04::Ledger ledger;ledger.impulse(100,0,0,out.acceptance*benefit);
    ledger.impulse(101,0,0,-out.moral);ledger.impulse(102,0,0,-out.devaluation*.35);
    ledger.impulse(103,0,0,-out.repetition);ledger.impulse(104,0,0,-refusal_cost);
    ledger.impulse(105,0,0,-time);
    if(norm_terms.group!=0)ledger.impulse(106,0,0,norm_terms.group);
    if(norm_terms.sanction!=0)ledger.impulse(107,0,0,norm_terms.sanction);
    out.score=cog04::squash(ledger.value(24,cog04::cfg::discount_per_hour));return out;
}
std::vector<SocialChoice> social_choices(const SocialView& v){
    std::vector<SocialChoice> out;
    if(!v.enabled||v.occupied)return out;
    auto add=[&](Interaction k,Id who,Id object,double urgency){if(v.memory.methods[ix(k)].mastery>=.6)out.push_back({k,who,object,urgency});};
    auto add_norm=[&](const NormPayload& payload,Id who){
        Interaction k=Interaction::ExplainPractice;
        if(payload.question)k=Interaction::AskPractice;
        else if(payload.explanation)k=Interaction::ExplainPractice;
        else if(payload.evidence.channel==NormChannel::Approval){
            if(payload.evidence.approval==ApprovalValue::Approve)k=Interaction::ApprovePractice;
            else if(payload.evidence.approval==ApprovalValue::Disapprove)k=Interaction::DisapprovePractice;
            else return;
        }
        if(!valid_norm_payload(k,payload))return;
        if(!payload.question&&payload.subject&&payload.subject!=who)return;
        double motive=norm_goal_value(v,payload.key);
        if(k==Interaction::DisapprovePractice)motive=std::max(motive,norm_personal_value(v,payload.key));
        if(motive<=0)return;
        if(v.memory.methods[ix(k)].mastery>=.6)out.push_back({k,who,0,motive,payload});
    };
    if(v.in_conversation&&v.partner)for(const auto& payload:v.norm_options)add_norm(payload,v.partner);
    if(v.in_conversation&&v.partner){
        if(v.life_enabled&&v.employed&&v.self==v.supervisor){
            const auto* previous=v.memory.experience(Interaction::RequestWork,v.partner,0,v.known_audience>0);
            if(!previous||v.now-previous->last>1800000)add(Interaction::RequestWork,v.partner,0,.7);
        }
        if(v.memory.community.enabled){
            for(const auto& memory:v.memory.community.entries){const auto score=topic_score(v,memory,v.partner);
                if(memory.content.id<=std::numeric_limits<Id>::max())add(Interaction::ShareNews,v.partner,Id(memory.content.id),.5+.25*score);}
            add(Interaction::DiscussTopic,v.partner,0,.40+.12*v.memory.community.curiosity);
        }
        if(v.civil_enabled){
            if(std::find(v.help_people.begin(),v.help_people.end(),v.partner)!=v.help_people.end()&&v.help_budget==0){const auto before=out.size();add(Interaction::AskMoney,v.partner,12,.9);if(out.size()>before)out.back().norm_payload=paid_payload(v,NormPractice::Help);}
            if(std::find(v.unknown_numbers.begin(),v.unknown_numbers.end(),v.partner)!=v.unknown_numbers.end())add(Interaction::AskPhone,v.partner,0,.55);
            for(Id recipe:v.teachable){const auto* old=v.memory.experience(Interaction::ExplainProcedure,v.partner,recipe,v.known_audience>0);if(!old||v.now-old->last>86400000){add(Interaction::ExplainProcedure,v.partner,recipe,.35);break;}}
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
    Id previous=0;for(const auto& item:m.items){check(item.id>previous,"item index");previous=item.id;check(unsigned(item.kind)<=unsigned(ObjectKind::Phone),"item kind");check(unsigned(item.owner_status)<=3&&unsigned(item.holder_status)<=3,"item status");require_range(item.expected_use,-1,1);require_range(item.prestige,0,1);}
    previous=0;for(const auto& p:m.people){check(p.id>previous,"person index");previous=p.id;check(unsigned(p.gender)<=2,"known gender");}
    check(std::is_sorted(m.processed.begin(),m.processed.end()),"processed index");check(std::adjacent_find(m.processed.begin(),m.processed.end())==m.processed.end(),"duplicate processed");
    check(std::is_sorted(m.experiences.begin(),m.experiences.end(),[](const auto& a,const auto& b){return key(a)<key(b);}),"context index");
    for(const auto& e:m.experiences){ix(e.kind);require_range(e.pleasure.mean,-1,1);require_range(e.approval.mean,-1,1);check(e.accepted>=1&&e.refused>=1,"acceptance counts");}
}
} // namespace life
