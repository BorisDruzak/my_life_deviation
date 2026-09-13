#include "life/resources.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>
#include <tuple>
namespace life {
void ResourceFact::validate()const{
    if(!root||!subject||unsigned(kind)>unsigned(ResourceKind::HelpReceived)||at<0)throw std::invalid_argument("resource fact identity/time");
    require_range(confidence,0,1);require_range(value,0,kind==ResourceKind::VisibleClothing?2:1e9);
}
bool ResourceKnowledge::receive(ResourceFact f,Id speaker,Tick now){
    f.validate();if(now<f.at)throw std::invalid_argument("future resource fact");
    if(f.root<=retired_root)return false;
    if(std::any_of(facts.begin(),facts.end(),[&](const auto& x){return x.root==f.root&&x.subject==f.subject&&x.kind==f.kind&&x.object==f.object;}))return false;
    f.speaker=speaker;f.reported=f.reported||speaker!=0;
    if(facts.size()==128){const auto it=std::min_element(facts.begin(),facts.end(),[](const auto& a,const auto& b){return a.root<b.root;});retired_root=std::max(retired_root,it->root);facts.erase(it);}
    facts.push_back(f);++revision;return true;
}
ResourceAssessment ResourceKnowledge::assess(Id subject,Tick now)const{
    if(now<0)throw std::invalid_argument("resource assessment clock");
    ResourceAssessment r;const ResourceFact *income=nullptr,*clothes=nullptr,*leisure=nullptr;
    auto update=[&](const ResourceFact*& old,const ResourceFact& f){if(!old||f.at>old->at||(f.at==old->at&&f.confidence>old->confidence))old=&f;};
    for(const auto& f:facts){if(f.subject!=subject||f.at>now)continue;
        const double q=f.confidence*std::exp2(-double(now-f.at)/(30.*86400000.));if(q<.2)continue;
        switch(f.kind){case ResourceKind::IncomeHourly:update(income,f);break;case ResourceKind::VisibleClothing:update(clothes,f);break;case ResourceKind::LeisureSpending:update(leisure,f);break;default:break;}}
    if(income){r.hourly=income->value;r.bases.push_back(income->root);}if(clothes){r.clothing_tier=clothes->value;r.bases.push_back(clothes->root);}if(leisure){r.leisure_spend=leisure->value;r.bases.push_back(leisure->root);}
    // A fallible personal rule, not proof of the subject's balance or willingness.
    // A single expensive item is insufficient; independent income and expenditure grounds are required.
    r.may_have_spare_money=income&&income->value>=18&&((leisure&&leisure->value>=15&&leisure->root!=income->root)||(clothes&&clothes->value>=1&&clothes->root!=income->root));
    std::sort(r.bases.begin(),r.bases.end());r.bases.erase(std::unique(r.bases.begin(),r.bases.end()),r.bases.end());return r;
}
void ResourceKnowledge::validate(Tick now)const{
    if(facts.size()>128)throw std::runtime_error("resource memory capacity");
    std::set<std::tuple<std::uint64_t,Id,ResourceKind,Id>> roots;
    for(const auto& f:facts){f.validate();if(f.at>now||!roots.insert({f.root,f.subject,f.kind,f.object}).second)throw std::runtime_error("resource provenance invariant");}
}
OpenQuestion* QuestionMemory::find(QuestionKind kind,Id target){return const_cast<OpenQuestion*>(std::as_const(*this).find(kind,target));}
const OpenQuestion* QuestionMemory::find(QuestionKind kind,Id target)const{auto i=std::find_if(items.begin(),items.end(),[&](const auto& q){return q.kind==kind&&q.target==target;});return i==items.end()?nullptr:&*i;}
OpenQuestion& QuestionMemory::ensure(QuestionKind kind,Id target,std::uint64_t basis,Tick now){
    if(!basis||now<0||unsigned(kind)>3)throw std::invalid_argument("question identity");
    if(auto* q=find(kind,target))return *q;
    if(items.size()==32){auto i=std::min_element(items.begin(),items.end(),[](const auto& a,const auto& b){return std::tie(a.pending,a.changed)<std::tie(b.pending,b.changed);});items.erase(i);++dropped;}
    OpenQuestion q;q.kind=kind;q.target=target;q.basis=basis;q.changed=now;items.push_back(q);return items.back();
}
bool QuestionMemory::wake(QuestionKind kind,Id target,std::uint64_t basis,Tick now){
    auto* old=find(kind,target);if(old&&old->basis==basis)return false;
    auto& q=ensure(kind,target,basis,now);if(now<q.changed)throw std::invalid_argument("question time reversal");
    q.basis=basis;q.changed=now;q.retry_at=now;q.pending=true;++q.revision;++reopened;return true;
}
void QuestionMemory::validate(Tick now)const{
    if(items.size()>32)throw std::runtime_error("question capacity");
    std::set<std::pair<QuestionKind,Id>> keys;
    for(const auto& q:items)if(!q.basis||q.changed>now||q.changed<0||q.retry_at<0||unsigned(q.kind)>3||!keys.insert({q.kind,q.target}).second)throw std::runtime_error("question invariant");
}
}
