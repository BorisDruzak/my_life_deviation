#include "life/civil.hpp"
#include "life/mind.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
namespace life {
double worn_condition(double old,double rate,double seconds){require_range(old,0,1);require_range(rate,0,1);require_range(seconds,0,1e12);return std::max(0.,old-rate*seconds/86400.);}
CivilBudget civil_budget(const CivilMemory& m,double money,int food,Tick now){
    require_range(money,0,1e12);if(food<0)throw std::invalid_argument("negative known food");
    if(now<0)throw std::invalid_argument("budget clock");
    // Known contract pays at 17:00 on working days. The reserve covers the gap,
    // plus a two-day precautionary buffer; it is derived from living costs.
    constexpr Tick day=86400000;Tick payday=(now/day)*day+17*3600000;
    if(payday<=now)payday+=day;
    while((payday/day)%7>=5)payday+=day;
    const int horizon=std::max(1,int((payday-now+day-1)/day));
    CivilBudget b;b.protected_cash=18*horizon+12*std::max(0,3*horizon-food)+2*(18+3*12);
    const double prices[]={35,95,180};if(m.desired_tier>2||m.garment_tier>2)throw std::invalid_argument("clothing tier");
    if(m.garment_condition<.45||m.desired_tier>m.garment_tier)b.clothing_goal=prices[m.desired_tier];
    b.leisure_goal=m.chosen_leisure_cost;b.target=b.protected_cash+b.clothing_goal+b.leisure_goal;
    b.pressure=unit((b.target-money)/std::max(1.,b.target));return b;
}
std::optional<std::uint64_t> CivilMemory::number_for(Id person)const{for(const auto& c:contacts)if(c.person==person)return c.number;return {};}
bool CivilMemory::learn_number(Id person,std::uint64_t number,std::uint64_t source,Tick now){
    if(!person||!number||!source||now<0)throw std::invalid_argument("phone address provenance");
    for(auto& c:contacts)if(c.person==person){if(c.number==number)return false;if(now<c.at)return false;c={person,number,source,now};questions.wake(QuestionKind::Contact,person,source,now);return true;}
    if(contacts.size()>=128)return false;
    contacts.push_back({person,number,source,now});questions.wake(QuestionKind::Contact,person,source,now);return true;
}
Id CivilMemory::compose(Id person,LetterContent content,std::uint64_t basis,Tick now){
    if(!person||!basis||now<0||unsigned(content.kind)>unsigned(LetterKind::NormPractice))throw std::invalid_argument("letter draft");
    if(!number_for(person)||drafts.size()>=16)return 0;
    for(const auto& d:drafts)if(d.person==person&&d.basis==basis&&d.content.kind==content.kind){
        if(content.kind!=LetterKind::NormPractice||
           (d.content.norm_payload.key==content.norm_payload.key&&
            d.content.norm_payload.evidence.source.revision==content.norm_payload.evidence.source.revision))return d.id;
    }
    if(next_draft==0)throw std::overflow_error("draft identifier");
    Draft d;d.id=next_draft++;d.person=person;d.content=std::move(content);d.basis=basis;d.created=now;drafts.push_back(d);return d.id;
}
void CivilMemory::record(CivilTrace t){trace.push_back(std::move(t));if(trace.size()>128)trace.erase(trace.begin());}
bool learn_procedure(ProjectMemory& p,LessonMemory& m,const ProcedureLesson& lesson,std::uint16_t known_steps,double understanding,Tick now){
    require_range(understanding,0,1);if(!lesson.root||!lesson.teacher||now<0)throw std::invalid_argument("procedure lesson source");
    ProjectMemory check;check.procedures={lesson.procedure};check.validate(now);
    if(lesson.root<=m.retired_root||std::any_of(m.lessons.begin(),m.lessons.end(),[&](const auto& x){return x.content.root==lesson.root;}))return false;
    StoredLesson s;s.content=lesson;s.at=now;s.understanding=understanding;
    for(const auto& step:lesson.procedure.steps)if(!(known_steps&(1u<<unsigned(step.kind))))s.missing_steps|=std::uint16_t(1u<<unsigned(step.kind));
    if(m.lessons.size()==32){m.retired_root=std::max(m.retired_root,m.lessons.front().content.root);m.lessons.erase(m.lessons.begin());}
    m.lessons.push_back(s);++m.received;
    if(!s.missing_steps&&understanding>=.6&&!p.known(lesson.procedure.id)&&p.procedures.size()<32){
        auto learned=lesson.procedure;learned.source=lesson.root;learned.mastery=std::min(.85,understanding);
        learned.successes=learned.failures=1; // communicated expectation is not personal practice
        p.procedures.push_back(std::move(learned));++p.revision;
    }
    return true;
}
}
namespace life {
namespace {
bool expanded_norm_strategy(NormMode mode){return mode!=NormMode::Legacy&&mode!=NormMode::Shadow;}
bool affordable(const PersonalView& v,double price){
    require_range(price,0,1e12);require_range(v.civil.profile.risk_importance,0,1);
    if(price>v.money)return false;
    return v.civil.profile.budget_policy==BudgetPolicy::Deliberative||price<=v.money-v.civil.budget.protected_cash;
}
NormPayload clothing_payload(const PersonalView& v,const RetailOffer& offer){
    NormPayload result;
    for(std::size_t i=0;i<v.norms_view.count;++i){const auto& prediction=v.norms_view.considered[i];
        if(prediction.key.practice==practice_id(NormPractice::ClothingTier)&&prediction.key.variant==offer.tier){result.present=true;result.key=prediction.key;return result;}}
    for(std::size_t i=0;i<v.norms_view.count;++i){const auto& prediction=v.norms_view.considered[i];
        if(prediction.key.practice==practice_id(NormPractice::ClothingCondition)){result.present=true;result.key=prediction.key;return result;}}
    return result;
}
}
std::vector<PlanOption> civil_options(const PersonalView& v){
    std::vector<PlanOption> out;const auto& c=v.civil;if(!c.enabled)return out;
    const bool shop_open=(v.now/86400000)%7<5&&v.now%86400000>=9*3600000&&v.now%86400000<17*3600000-5*60000;
    std::vector<PlanOption> clothing;
    for(const auto& shop:c.memory.shops){const auto* q=c.memory.questions.find(QuestionKind::Clothing,shop.place);
        if(!q||!q->actionable||q->pending||v.now<shop.retry_at||!shop_open)continue;
        for(const auto& offer:shop.offers){
            if((!expanded_norm_strategy(v.norms_view.mode)&&offer.tier!=c.memory.desired_tier)||!affordable(v,offer.price))continue;
            PlanOption p;p.method=Method::BuyClothes;p.place=shop.place;p.object=offer.sku;p.salience=.65+.3*(1-c.memory.garment_condition);p.norm_payload=clothing_payload(v,offer);clothing.push_back(p);}
        // When a desired upgrade is unaffordable, a worn garment can be replaced by a basic one.
        if(!expanded_norm_strategy(v.norms_view.mode)&&c.memory.garment_condition<.3&&c.memory.desired_tier>0)for(const auto& offer:shop.offers)if(offer.tier==0&&affordable(v,offer.price)){PlanOption p;p.method=Method::BuyClothes;p.place=shop.place;p.object=offer.sku;p.salience=.8;clothing.push_back(p);}
    }
    std::stable_sort(clothing.begin(),clothing.end(),[](const auto& a,const auto& b){return std::tie(a.place,a.object)<std::tie(b.place,b.object);});
    clothing.erase(std::unique(clothing.begin(),clothing.end(),[](const auto& a,const auto& b){return a.place==b.place&&a.object==b.object;}),clothing.end());
    if(expanded_norm_strategy(v.norms_view.mode)&&clothing.size()>3)clothing.resize(3);
    out.insert(out.end(),clothing.begin(),clothing.end());
    if(c.can_text){
        for(const auto& draft:c.memory.drafts)if(v.now>=draft.retry_at&&c.memory.number_for(draft.person)){PlanOption p;p.method=Method::SendMessage;p.place=v.place;p.partner=draft.person;p.object=draft.id;p.salience=draft.content.kind==LetterKind::CancelMeeting?.95:.6;out.push_back(p);break;}
        if(!c.memory.inbox.empty()&&c.memory.inbox.front()<=0xffffffffu){PlanOption p;p.method=Method::ReadMessage;p.place=v.place;p.object=Id(c.memory.inbox.front());p.salience=.75;out.push_back(p);}
    }
    if(v.money<c.budget.protected_cash&&!v.social.in_conversation){
        for(Id other:c.help_candidates)if(std::find(v.perceived_people.begin(),v.perceived_people.end(),other)!=v.perceived_people.end()){
            const auto* q=c.memory.questions.find(QuestionKind::Money,other);if(q&&q->actionable){out.push_back({Method::Talk,v.place,other,.85});break;}}
    }
    return out;
}
double civil_expected_gain(const PersonalView& v,const PlanOption& p){
    const auto& c=v.civil;if(!c.enabled)return -1;
    if(p.method==Method::BuyClothes){
        for(const auto& shop:c.memory.shops)if(shop.place==p.place)for(const auto& offer:shop.offers)if(offer.sku==p.object){
            if(!affordable(v,offer.price))return -1;
            const double repair=c.memory.garment_condition<.4?.85+.15*(.4-c.memory.garment_condition)/.4:0;
            const double presentation=!expanded_norm_strategy(v.norms_view.mode)&&offer.tier>c.memory.garment_tier?.45*v.social.memory.status_importance:0;
            return std::max(repair,presentation); // no double reward for the same replacement
        }return -1;
    }
    if(!c.can_text)return -1;
    if(p.method==Method::ReadMessage)return .40;
    if(p.method==Method::SendMessage){for(const auto& d:c.memory.drafts)if(d.id==p.object)return d.content.kind==LetterKind::CancelMeeting?.65:.20+.25*v.need[5];}
    return -1;
}
}
