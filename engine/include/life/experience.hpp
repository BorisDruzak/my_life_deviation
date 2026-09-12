#pragma once
#include "life/mind.hpp"
namespace life {
struct DecisionExperience {
    std::uint64_t action=0,thought=0,knowledge_source=0;
    Tick at=0;SelfDomain domain=SelfDomain::General;
    double predicted_uncertainty=0,predicted_risk=0,importance=0;
    SelfPrediction self_prediction;
    bool known=false,intentional=true;
    template<class A>void fields(A& a){a(action,thought,knowledge_source,at,domain,predicted_uncertainty,predicted_risk,importance,self_prediction,known,intentional);}
};
struct OutcomeSignal {
    std::uint64_t id=0,root=0,action=0;
    Method method=Method::Idle;Interaction interaction=Interaction::FriendlyTouch;
    Tick at=0;Outcomes observed{};std::uint16_t observed_mask=0;
    Id other=0;bool completed=false,blocked=false,interrupted=false,intentional=true,feedback_observed=false;
    double directness=1,goal_importance=.3;
    // Only outwardly observed feedback belongs here. Never copy a private
    // refusal threshold or the executor's full error/debug message.
    double action_feedback=0,other_decision=0,observed_obstacle=0;
    bool acceptance_observed=false;double accepted=0;
    DecisionExperience decision;
    SelfEpisodeStage stage=SelfEpisodeStage::Outcome;
    SelfDomain recovery_domain=SelfDomain::Recovery;
    bool recovery_observed=false;double recovered=0,functioning=0,consequences=0;
    Tick elapsed_recovery_ms=0;
    template<class A>void fields(A& a){a(id,root,action,method,interaction,at,observed,observed_mask,other,completed,blocked,interrupted,intentional,feedback_observed,directness,goal_importance,action_feedback,other_decision,observed_obstacle,acceptance_observed,accepted,decision,stage,recovery_domain,recovery_observed,recovered,functioning,consequences,elapsed_recovery_ms);}
};
struct InterpretationContext {
    SelfPrediction self;
    double causal_clarity=.5,perception_quality=1,rejection_bias=.35;
    double prior_causal_control=.5;
};
InterpretedEpisode interpret_outcome(const OutcomeSignal& signal,const InterpretationContext& context);
Attribution attribute_outcome(const InterpretedEpisode& episode,const OutcomeSignal& signal,const InterpretationContext& context);
Appraisal self_appraisal(const SelfPrediction& self,double importance,double expected_harm,double uncertainty);
double outcome_priority(const OutcomeSignal& signal);
void validate_outcome(const OutcomeSignal& signal);
} // namespace life
