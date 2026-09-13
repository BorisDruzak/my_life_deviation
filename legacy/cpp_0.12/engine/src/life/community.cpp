#include "life/community.hpp"
#include "life/numeric.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>

namespace life {
double MemoryDetail::at(Tick now)const {
    if(now<anchored||!std::isfinite(strength)||strength<0||strength>1||!std::isfinite(half_life_hours)||half_life_hours<=0)
        throw std::invalid_argument("invalid memory interval/strength");
    return strength*std::exp2(-double(now-anchored)/(3600000.*half_life_hours));
}
void MemoryDetail::reinforce(Tick now,double amount){require_range(amount,0,1);strength=at(now);strength+=amount*(1-strength);anchored=now;}
const NewsMemory* CommunityMemory::find(std::uint64_t id)const {
    auto it=std::lower_bound(entries.begin(),entries.end(),id,[](const auto& m,auto x){return m.content.id<x;});
    return it!=entries.end()&&it->content.id==id?&*it:nullptr;
}
bool CommunityMemory::receive(const Information& f,Id speaker,std::uint64_t delivery,Tick now){
    if(!f.id||!delivery||f.occurred_at>now||now<0||unsigned(f.kind)>=unsigned(NewsKind::Count)||unsigned(f.disclosure)>2||unsigned(f.origin)>2)throw std::invalid_argument("invalid information");
    for(double x:{f.confidence,f.importance,f.sensitivity})require_range(x,0,1);
    require_range(f.valence,-1,1);require_range(f.material,0,1);
    auto d=std::lower_bound(delivered.begin(),delivered.end(),delivery);
    if(d!=delivered.end()&&*d==delivery)return false;
    delivered.insert(d,delivery);++received;
    auto it=std::lower_bound(entries.begin(),entries.end(),f.id,[](const auto& m,auto x){return m.content.id<x;});
    if(it!=entries.end()&&it->content.id==f.id){
        // Same narrative is not an independent witness. Receiving it again can
        // refresh accessibility, but cannot upgrade truth/confidence/provenance.
        it->gist.reinforce(now,.15);++revision;return true;
    }
    NewsMemory m;m.content=f;m.speaker=speaker;m.first_delivery=delivery;m.encoded_at=now;
    if(speaker){m.content.origin=NewsOrigin::Reported;}
    m.gist={1,12*(1+6*f.importance),now};m.place={1,.5*(1+f.importance),now};
    m.time={1,1*(1+f.importance),now};m.source={1,24*(1+3*f.importance),now};
    entries.insert(it,std::move(m));++revision;return true;
}
std::optional<Information> CommunityMemory::recall(std::uint64_t id,Tick now)const{
    const auto* m=find(id);if(!m||m->gist.at(now)<.08)return {};
    auto f=m->content;
    if(m->place.at(now)<.2)f.location=0;
    if(m->time.at(now)<.2)f.occurred_at=-1; // unknown time, not an invented zero
    if(m->source.at(now)<.2)f.cited_source=0;
    return f;
}
void CommunityMemory::rehearse(std::uint64_t id,Tick now){
    auto it=std::lower_bound(entries.begin(),entries.end(),id,[](const auto& m,auto x){return m.content.id<x;});
    if(it==entries.end()||it->content.id!=id||it->gist.at(now)<.08)return;
    it->gist.reinforce(now,.1);++retrievals;++revision;
}
void CommunityMemory::forget(Tick now){
    const auto old=entries.size();
    std::erase_if(entries,[&](const auto& m){return !m.pinned&&m.gist.at(now)<.08;});
    forgotten+=old-entries.size();if(entries.size()!=old)++revision;
    // Sharing history is private behavioral memory, not transport replay protection.
    std::erase_if(shared,[&](const auto& x){return now-x.at>7*86400000LL;});
}
std::vector<NewsMemory> CommunityMemory::candidates(Tick now,std::size_t limit)const{
    if(limit>64)throw std::invalid_argument("memory retrieval budget");
    std::vector<NewsMemory> out;
    // Bounded hot result; ordered insertion avoids copying the whole archive.
    for(const auto& m:entries){const double access=m.gist.at(now);if(access<.08)continue;
        auto score=[&](const NewsMemory& x){return x.gist.at(now)*(.2+x.content.importance);};
        const auto it=std::lower_bound(out.begin(),out.end(),m,[&](const auto& x,const auto& y){const double a=score(x),b=score(y);return a!=b?a>b:x.content.id<y.content.id;});
        if(out.size()<limit||it!=out.end()){out.insert(it,m);if(out.size()>limit)out.pop_back();}
    }
    std::sort(out.begin(),out.end(),[](const auto& x,const auto& y){return x.content.id<y.content.id;});return out;
}
double CommunityMemory::disclosure_cost(const Information& f,double trust)const{
    require_range(trust,0,1);require_range(confidentiality,0,1);
    const double norm=f.disclosure==Disclosure::Entrusted?1:f.disclosure==Disclosure::Personal?.45:0;
    return confidentiality*norm+.3*f.sensitivity*(1-trust);
}
double CommunityMemory::topic_score(const NewsMemory& m,Id listener,Tick now)const{
    double repeated=0;
    for(const auto& s:shared)if(s.claim==m.content.id&&s.listener==listener)repeated+=.5*std::exp2(-double(std::max<Tick>(0,now-s.at))/3600000.);
    return (.15+.3*curiosity+.4*m.content.importance)*m.gist.at(now)-disclosure_cost(m.content)-repeated;
}
ReputationView CommunityMemory::opinion(Id person,Tick now)const{
    ReputationView out;double sm=0,wm=0,sh=0,wh=0,so=0,wo=0;
    for(const auto& m:entries){if(m.content.subject!=person)continue;const double access=m.gist.at(now);if(access<.08)continue;
        const double w=m.content.confidence*access;
        if(m.content.kind==NewsKind::Employment||m.content.kind==NewsKind::Purchase){sm+=w*m.content.material;wm+=w;}
        if(m.content.kind==NewsKind::Help){sh+=w*m.content.valence;wh+=w;}
        if(m.content.kind==NewsKind::Opinion){so+=w*m.content.valence;wo+=w;}
        ++out.sources;
    }
    if(wm){out.material=sm/wm;} if(wh){out.helpfulness=sh/wh;} if(wo){out.honesty=so/wo;}
    out.coverage=double(bool(wm)+bool(wh)+bool(wo))/3;return out;
}
double attraction(const Appearance& a,const Preference& p){
    require_range(a.height_cm,120,220);require_range(a.build,0,1);if(a.hair>=4)throw std::invalid_argument("hair");
    require_range(p.preferred_height,120,220);require_range(p.preferred_build,0,1);require_range(p.height_weight,0,1);require_range(p.build_weight,0,1);
    if(p.height_weight+p.build_weight>1)throw std::invalid_argument("preference weight sum");
    for(double x:p.hair_liking)require_range(x,0,1);
    const double h=std::exp(-std::pow((a.height_cm-p.preferred_height)/25,2));
    const double b=std::max(0.,1-std::abs(a.build-p.preferred_build));
    return unit(p.height_weight*h+p.build_weight*b+(1-p.height_weight-p.build_weight)*p.hair_liking[a.hair]);
}
const std::array<JobOffer,4>& job_catalogue(){
    static const std::array<JobOffer,4> jobs{{{"assistant",12.5,701},{"qualified",18.75,702},{"specialist",28.75,703},{"coordinator",42.5,704}}};return jobs;
}
const std::array<Recreation,6>& recreation_catalogue(){
    static const std::array<Recreation,6> r{{{"walk",0,.45,.06},{"reading",2,.46,0},{"board_game",5,.48,.02},{"sport",8,.5,.25},{"cinema",15,.48,0},{"club",25,.48,.02}}};return r;
}
}
