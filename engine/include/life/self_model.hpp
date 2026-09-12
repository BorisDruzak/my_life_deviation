#pragma once
#include "life/semantic_types.hpp"
#include <array>
#include <functional>
#include <cstddef>

namespace life {
enum class Method : std::uint8_t;
enum class Interaction : std::uint8_t;
enum class SelfDomain : std::uint8_t {General,Social,Romance,Conflict,Work,Study,Risk,Recovery,Count};
enum class SelfAxis : std::uint8_t {Efficacy,Control,Coping,Acceptance,UncertaintyTolerance,Count};
constexpr std::size_t self_domain_count=std::size_t(SelfDomain::Count);
constexpr std::size_t self_axis_count=std::size_t(SelfAxis::Count);
namespace self_cfg {
constexpr double generalization=.20, general_prior_weight=.10;
constexpr Tick recovery_delay=1800000, recovery_expiry=7*86400000LL;
constexpr std::size_t source_capacity=128, inbox_capacity=32, recovery_capacity=8;
}
// A bounded exact hot set plus a conservative replay floor. IDs at/below a
// retired floor are never accepted again. This is an explicit admission policy
// for excessively late inputs, not a claim that a finite cache remembers all IDs.
struct SelfSources {
    std::array<std::uint64_t,self_cfg::source_capacity> roots{};
    std::uint16_t count=0; std::uint64_t retired_through=0;
    bool contains(std::uint64_t source) const;
    bool insert(std::uint64_t source);
    std::size_t size() const {return count;}
    void validate() const;
    template<class A>void fields(A& a){a(roots,count,retired_through);}
};
struct SelfBelief {
    Estimate learned{.5,.25,0}; double prior=.5;
    std::array<std::uint64_t,8> recent_roots{};
    std::uint8_t root_count=0,root_cursor=0;
    double value() const;
    double confidence() const;
    bool observe(double value,double learning,double quality,double dose,double memory,std::uint64_t source);
    void validate() const;
    template<class A>void fields(A& a){a(learned,prior,recent_roots,root_count,root_cursor);}
};
struct SelfSchema {
    SelfDomain domain=SelfDomain::General;
    std::array<SelfBelief,self_axis_count> beliefs{};
    Tick updated=0;
    template<class A>void fields(A& a){a(domain,beliefs,updated);}
};
struct SelfPrediction {
    double efficacy=.5,control=.5,coping=.5,acceptance=.5,uncertainty_tolerance=.5,confidence=0;
    std::array<double,self_axis_count> axis_confidence{};
    void validate() const;
    template<class A>void fields(A& a){a(efficacy,control,coping,acceptance,uncertainty_tolerance,confidence,axis_confidence);}
};
struct Attribution {
    double self=0,other=0,environment=0,chance=1;
    double controllability=.5,confidence=0;
    void validate() const;
    template<class A>void fields(A& a){a(self,other,environment,chance,controllability,confidence);}
};
enum class SelfEpisodeStage:std::uint8_t {Outcome,Recovery};
struct InterpretedEpisode {
    std::uint64_t id=0,source_event=0,source_action=0;
    SelfDomain domain=SelfDomain::General;
    Method method=static_cast<Method>(0); Tick time=0;
    double success=.5,importance=0,adversity=0,uncertainty=0,perception_quality=0;
    Attribution attribution;
    bool intentional=false,actual=false;
    Id other=0;
    bool acceptance_observed=false;double accepted=0;
    SelfEpisodeStage stage=SelfEpisodeStage::Outcome;
    bool recovery_observed=false;double recovered=0,functioning=0,consequences=0;
    Tick elapsed_recovery_ms=0;
    // Personal interpretation, not a mirror of the executor's true cause.
    double causal_clarity=0;std::uint64_t decision_source=0;
    template<class A>void fields(A& a){a(id,source_event,source_action,domain,method,time,success,importance,adversity,uncertainty,perception_quality,attribution,intentional,actual,other,acceptance_observed,accepted,stage,recovery_observed,recovered,functioning,consequences,elapsed_recovery_ms,causal_clarity,decision_source);}
};
struct SelfUpdateTrace {
    Id actor=0;std::uint64_t episode=0;SelfDomain domain=SelfDomain::General;SelfAxis axis=SelfAxis::Efficacy;
    double before=0,observation=0,after=0,attribution_self=0,quality=0;std::uint64_t source=0;
    SelfEpisodeStage stage=SelfEpisodeStage::Outcome;
};
using SelfTraceSink=std::function<void(const SelfUpdateTrace&)>;
struct SelfRecoveryOrigin {
    std::uint64_t source=0,action=0;SelfDomain domain=SelfDomain::General;
    Tick at=0;double importance=0;
    template<class A>void fields(A& a){a(source,action,domain,at,importance);}
};
struct SelfModel {
    std::array<SelfSchema,self_domain_count> schemas{};
    SelfSources primary_sources,recovery_sources;
    std::array<SelfRecoveryOrigin,self_cfg::recovery_capacity> recovery_origins{};
    SelfModel();
    SelfPrediction predict(SelfDomain domain) const;
    bool integrate(const InterpretedEpisode& episode,const Cognitive& cognitive,double memory,const SelfTraceSink& trace={});
    void validate() const;
    template<class A>void fields(A& a){a(schemas,primary_sources,recovery_sources,recovery_origins);}
};
// A prospective trace records what was actually experienced. It does not
// award coping or mark recovery merely because a timer has elapsed.
struct RecoveryTrace {
    std::uint64_t episode=0,action=0;SelfDomain domain=SelfDomain::General;
    Tick started=0,evaluate_after=0,expires=0;
    double initial_distress=0,importance=0,uncertainty=0,perception_quality=0;
    double initial_damage=0;std::uint64_t functioning_at_start=0,activities_at_start=0;
    bool active=false;
    template<class A>void fields(A& a){a(episode,action,domain,started,evaluate_after,expires,initial_distress,importance,uncertainty,perception_quality,initial_damage,functioning_at_start,activities_at_start,active);}
};
struct SelfCost {
    double failure_exposure=0,uncertainty_cost=0,helplessness_cost=0;
    double total()const{return failure_exposure+uncertainty_cost+helplessness_cost;}
};
SelfCost self_cost(const SelfPrediction& self,double importance,double uncertainty,double exposure,double specific_confidence=0);
const char* self_domain_name(SelfDomain domain);
const char* self_axis_name(SelfAxis axis);
SelfDomain domain_of(Method method);
SelfDomain domain_of(Interaction interaction);
} // namespace life
