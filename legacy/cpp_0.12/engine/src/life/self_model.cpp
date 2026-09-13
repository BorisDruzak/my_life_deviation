#include "life/self_model.hpp"
#include "life/mind.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace life {
namespace {
void index_ok(SelfDomain d){if(std::size_t(d)>=self_domain_count)throw std::invalid_argument("invalid SelfDomain");}
void validate_episode(const InterpretedEpisode& e){
    index_ok(e.domain);
    if(!e.source_action||std::size_t(e.method)>=method_count)throw std::invalid_argument("self action provenance/method");
    if(unsigned(e.stage)>unsigned(SelfEpisodeStage::Recovery))throw std::invalid_argument("invalid self episode stage");
    for(double x:{e.success,e.importance,e.adversity,e.uncertainty,e.perception_quality,e.accepted,e.recovered,e.functioning,e.consequences,e.causal_clarity})require_range(x,0,1);
    if(e.elapsed_recovery_ms<0)throw std::invalid_argument("negative recovery interval");
    e.attribution.validate();
}
}
const char* self_domain_name(SelfDomain d){index_ok(d);static const char* n[]={"general","social","romance","conflict","work","study","risk","recovery"};return n[std::size_t(d)];}
const char* self_axis_name(SelfAxis a){if(std::size_t(a)>=self_axis_count)throw std::invalid_argument("invalid SelfAxis");static const char* n[]={"efficacy","control","coping","acceptance","uncertainty_tolerance"};return n[std::size_t(a)];}
bool SelfSources::contains(std::uint64_t id)const{
    return id&&(id<=retired_through||std::binary_search(roots.begin(),roots.begin()+count,id));
}
bool SelfSources::insert(std::uint64_t id){
    if(!id||contains(id))return false;
    if(count==roots.size()){
        if(id<roots.front())return false;
        // Retire the lowest accepted root. The pending inbox and recovery
        // descriptors have their own identities; they never replay a primary.
        retired_through=roots.front();std::move(roots.begin()+1,roots.end(),roots.begin());--count;
        if(id<=retired_through)return false;
    }
    auto at=std::lower_bound(roots.begin(),roots.begin()+count,id);
    std::move_backward(at,roots.begin()+count,roots.begin()+count+1);*at=id;++count;return true;
}
void SelfSources::validate()const{
    if(count>roots.size())throw std::runtime_error("self source capacity");
    std::uint64_t last=retired_through;
    for(std::size_t i=0;i<count;++i){if(roots[i]<=last)throw std::runtime_error("self duplicate/unordered source");last=roots[i];}
    for(std::size_t i=count;i<roots.size();++i)if(roots[i])throw std::runtime_error("self unused source slot");
}
double SelfBelief::confidence()const{return learned.confidence();}
double SelfBelief::value()const{const double c=confidence();return (1-c)*prior+c*unit(learned.mean);}
bool SelfBelief::observe(double v,double learning,double quality,double dose,double memory,std::uint64_t source){
    for(double x:{v,learning,quality,dose,memory})require_range(x,0,1);
    if(!source)throw std::invalid_argument("zero self evidence source");
    if(quality==0||dose==0||learning==0)return false;
    if(std::find(recent_roots.begin(),recent_roots.begin()+root_count,source)!=recent_roots.begin()+root_count)return false;
    // Estimate's generic default mean is zero; self beliefs start at their prior.
    if(learned.count==0)learned.mean=prior;
    learned.observe(v,learning,quality,dose,memory,true);
    recent_roots[root_cursor]=source;root_cursor=std::uint8_t((root_cursor+1)%recent_roots.size());
    root_count=std::min<std::uint8_t>(8,std::uint8_t(root_count+1));return true;
}
void SelfBelief::validate()const{
    require_range(prior,0,1);require_range(learned.mean,0,1);require_range(learned.variance,0,1);require_range(learned.count,0,1e15);
    if(root_count>8||root_cursor>=8||(root_count<8&&root_cursor!=root_count))throw std::runtime_error("invalid self provenance cursor");
    for(unsigned i=0;i<root_count;++i){if(!recent_roots[i])throw std::runtime_error("zero self evidence");for(unsigned j=0;j<i;++j)if(recent_roots[j]==recent_roots[i])throw std::runtime_error("duplicate self evidence");}
    if(learned.count>0&&!root_count)throw std::runtime_error("self estimate without provenance");
}
void SelfPrediction::validate()const{
    for(double x:{efficacy,control,coping,acceptance,uncertainty_tolerance,confidence})require_range(x,0,1);
    for(double x:axis_confidence)require_range(x,0,1);
}
void Attribution::validate()const{
    for(double x:{self,other,environment,chance,controllability,confidence})require_range(x,0,1);
    if(std::abs(self+other+environment+chance-1)>1e-9)throw std::invalid_argument("attribution must sum to one");
}
SelfModel::SelfModel(){for(unsigned i=0;i<self_domain_count;++i)schemas[i].domain=SelfDomain(i);}
SelfPrediction SelfModel::predict(SelfDomain d)const{
    index_ok(d);SelfPrediction p;
    std::array<double,self_axis_count> v{};
    const auto& own=schemas[std::size_t(d)];
    for(unsigned i=0;i<self_axis_count;++i){
        const auto& b=own.beliefs[i];v[i]=b.value();p.axis_confidence[i]=b.confidence();
        if(d!=SelfDomain::General){
            const auto& g=schemas[0].beliefs[i];
            v[i]=unit(v[i]+self_cfg::general_prior_weight*(1-b.confidence())*(g.value()-g.prior));
        }
        p.confidence+=p.axis_confidence[i]/self_axis_count;
    }
    p.efficacy=v[0];p.control=v[1];p.coping=v[2];p.acceptance=v[3];p.uncertainty_tolerance=v[4];return p;
}
bool SelfModel::integrate(const InterpretedEpisode& e,const Cognitive& c,double memory,const SelfTraceSink& trace){
    if(!e.actual||!e.id||!e.source_event)return false;
    validate_episode(e);require_range(memory,0,1);require_range(c.learnability,0,1);require_range(c.plasticity,0,1);
    const double q=e.perception_quality*e.importance;
    if(q<=0||c.learnability<=0)return false;
    const bool recovery=e.stage==SelfEpisodeStage::Recovery;
    auto origin=std::find_if(recovery_origins.begin(),recovery_origins.end(),[&](const auto& x){return x.source==e.source_event;});
    if(recovery&&(!e.recovery_observed||e.elapsed_recovery_ms<self_cfg::recovery_delay||origin==recovery_origins.end()||
        origin->domain!=e.domain||origin->action!=e.source_action||e.time-origin->at<self_cfg::recovery_delay))return false;
    auto& roots=recovery?recovery_sources:primary_sources;
    if(roots.contains(e.source_event))return false;
    // No writes precede validation and replay rejection.
    if(!roots.insert(e.source_event))return false;
    if(!recovery&&e.adversity>=.2){
        auto free=std::find_if(recovery_origins.begin(),recovery_origins.end(),[&](const auto& x){return !x.source||e.time-x.at>self_cfg::recovery_expiry;});
        if(free==recovery_origins.end())free=std::min_element(recovery_origins.begin(),recovery_origins.end(),[](const auto& a,const auto& b){return a.importance!=b.importance?a.importance<b.importance:a.at<b.at;});
        if(!free->source||e.time-free->at>self_cfg::recovery_expiry||free->importance<=e.importance)*free={e.source_event,e.source_action,e.domain,e.time,e.importance};
    }
    if(recovery)*origin=SelfRecoveryOrigin{};
    bool changed=false;
    auto update=[&](SelfDomain domain,SelfAxis axis,double observation,double quality){
        if(quality<=0)return;
        auto& schema=schemas[std::size_t(domain)];auto& b=schema.beliefs[std::size_t(axis)];const double before=b.value();
        if(b.observe(observation,c.learnability,quality,1,memory,e.source_event)){
            schema.updated=std::max(schema.updated,e.time);changed=true;
            if(trace)trace({0,e.id,domain,axis,before,observation,b.value(),e.attribution.self,quality,e.source_event,e.stage});
        }
    };
    auto axis=[&](SelfAxis kind,double value,double quality){
        update(e.domain,kind,value,quality);
        if(e.domain!=SelfDomain::General)update(SelfDomain::General,kind,value,quality*self_cfg::generalization);
    };
    if(!recovery){
        if(e.intentional)axis(SelfAxis::Efficacy,e.success,q*e.attribution.self);
        axis(SelfAxis::Control,e.attribution.controllability,q*e.attribution.confidence);
        if(e.acceptance_observed&&(e.domain==SelfDomain::Social||e.domain==SelfDomain::Romance||e.domain==SelfDomain::Conflict))
            axis(SelfAxis::Acceptance,e.accepted,q);
        // A benign uncertain result can be experienced as tolerable immediately.
        // Adversity instead requires the independent delayed recovery observation.
        if(e.intentional&&e.uncertainty>0&&e.adversity<.2)
            axis(SelfAxis::UncertaintyTolerance,1-e.adversity,q*e.uncertainty);
    }else{
        const double coped=unit((.65*e.recovered+.35*(1-e.consequences))*(.4+.6*e.functioning));
        axis(SelfAxis::Coping,coped,q);
        if(e.uncertainty>0)axis(SelfAxis::UncertaintyTolerance,coped,q*e.uncertainty);
    }
    return changed;
}
void SelfModel::validate()const{
    primary_sources.validate();recovery_sources.validate();
    for(unsigned i=0;i<recovery_origins.size();++i){const auto& x=recovery_origins[i];if(!x.source)continue;
        index_ok(x.domain);require_range(x.importance,0,1);if(!x.action||!primary_sources.contains(x.source))throw std::runtime_error("unproven recovery source");
        for(unsigned j=0;j<i;++j)if(recovery_origins[j].source==x.source)throw std::runtime_error("duplicate recovery source");}
    for(unsigned i=0;i<self_domain_count;++i){if(schemas[i].domain!=SelfDomain(i))throw std::runtime_error("self domain slot mismatch");for(const auto& b:schemas[i].beliefs)b.validate();}
}
SelfCost self_cost(const SelfPrediction& p,double importance,double uncertainty,double exposure,double specific_confidence){
    for(double x:{p.efficacy,p.control,p.coping,p.acceptance,p.uncertainty_tolerance,p.confidence,importance,uncertainty,exposure,specific_confidence})require_range(x,0,1);
    // Costs apply to uncertain personally consequential exposure, not to the
    // physical efficacy of eating/drinking. Specific experience displaces a
    // general self-schema instead of being charged for the same outcome twice.
    p.validate();
    const double q=importance*exposure*(1-specific_confidence);
    return {q*(1-p.efficacy)*(1-p.coping),q*uncertainty*(1-p.uncertainty_tolerance),q*.25*(1-p.control)};
}
} // namespace life
