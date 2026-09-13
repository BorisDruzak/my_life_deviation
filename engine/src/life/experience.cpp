#include "life/experience.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace life {
SelfDomain domain_of(Method m){
    if(std::size_t(m)>=method_count)throw std::invalid_argument("invalid method for self domain");
    switch(m){
        case Method::Talk:case Method::Social:return SelfDomain::Social;
        case Method::PrivateIntimacy:return SelfDomain::Romance;
        case Method::Work:case Method::InquireJob:case Method::ApplyJob:return SelfDomain::Work;
        case Method::Study:return SelfDomain::Study;
        case Method::TakeFood:return SelfDomain::Risk;
        default:return SelfDomain::General;
    }
}
SelfDomain domain_of(Interaction i){
    if(std::size_t(i)>=interaction_count)throw std::invalid_argument("invalid interaction for self domain");
    switch(i){case Interaction::RomanticTouch:case Interaction::PartnerIntimacy:return SelfDomain::Romance;
        case Interaction::ClaimItem:return SelfDomain::Conflict;
        case Interaction::RequestWork:return SelfDomain::Work;
        default:return SelfDomain::Social;}
}
void validate_outcome(const OutcomeSignal& s){
    if(!s.id||!s.root||!s.action||std::size_t(s.method)>=method_count||std::size_t(s.interaction)>=interaction_count||s.observed_mask>=(1u<<metric_count))throw std::invalid_argument("invalid outcome identity/mask");
    for(double x:{s.directness,s.goal_importance,s.action_feedback,s.other_decision,s.observed_obstacle,s.accepted,s.recovered,s.functioning,s.consequences})require_range(x,0,1);
    for(double x:s.observed)require_range(x,-1,1);
    if(std::size_t(s.recovery_domain)>=self_domain_count)throw std::invalid_argument("invalid recovery domain");
    if(s.decision.known){
        s.decision.self_prediction.validate();
        if(std::size_t(s.decision.domain)>=self_domain_count)throw std::invalid_argument("decision self domain");
        for(double x:{s.decision.predicted_uncertainty,s.decision.predicted_risk,s.decision.importance})require_range(x,0,1);
        if(s.decision.action!=s.action||s.decision.at>s.at)throw std::invalid_argument("outcome decision provenance mismatch");
    }
    if(unsigned(s.stage)>unsigned(SelfEpisodeStage::Recovery)||s.elapsed_recovery_ms<0)throw std::invalid_argument("outcome stage/time");
}
InterpretedEpisode interpret_outcome(const OutcomeSignal& s,const InterpretationContext& c){
    validate_outcome(s);
    for(double x:{c.causal_clarity,c.perception_quality,c.rejection_bias,c.prior_causal_control})require_range(x,0,1);
    InterpretedEpisode e;e.id=s.id;e.source_event=s.root;e.source_action=s.action;e.method=s.method;
    e.time=s.at;e.domain=s.method==Method::Social?domain_of(s.interaction):domain_of(s.method);
    if(s.stage==SelfEpisodeStage::Recovery)e.domain=s.recovery_domain;
    e.intentional=s.intentional;e.actual=s.directness>0&&(s.feedback_observed||s.observed_mask||s.recovery_observed);
    e.perception_quality=s.directness*c.perception_quality;e.importance=s.goal_importance;e.other=s.other;
    e.success=s.completed?1:s.interrupted?.5:0;
    int goal_metric=-1;double typical=1;
    switch(s.method){
      case Method::Eat:goal_metric=0;typical=.35;break;
      case Method::Drink:goal_metric=1;typical=.25;break;
      case Method::Sleep:goal_metric=2;typical=.3;break;
      case Method::Rest:goal_metric=3;typical=.2;break;
      case Method::Leisure:goal_metric=4;typical=.15;break;
      case Method::Talk:goal_metric=5;typical=.3;break;
      case Method::PrivateIntimacy:goal_metric=6;typical=.3;break;
      default:break;
    }
    if(goal_metric>=0&&(s.observed_mask&(1u<<goal_metric))){
        e.success=unit(s.observed[unsigned(goal_metric)]/typical);
        e.adversity=.2*s.goal_importance*(1-e.success);
    }
    if(s.observed_mask&(1u<<std::size_t(Metric::Pleasantness))){
        const auto pleasure=s.observed[std::size_t(Metric::Pleasantness)];
        if(pleasure<0)e.success=std::min(e.success,unit(.5+pleasure));
        e.adversity=std::max(e.adversity,std::max(0.,-pleasure));
    }
    if(s.blocked)e.adversity=std::max(e.adversity,s.goal_importance*(.3+.4*c.rejection_bias));
    e.acceptance_observed=s.acceptance_observed;e.accepted=s.accepted;
    if(s.acceptance_observed){e.success=s.accepted;e.adversity=std::max(e.adversity,(1-s.accepted)*s.goal_importance*(.2+.6*c.rejection_bias));}
    // Unknown predecision uncertainty is not inferred from the outcome.
    if(s.decision.known){
        s.decision.self_prediction.validate();
        if(std::size_t(s.decision.domain)>=self_domain_count)throw std::invalid_argument("decision self domain");
        e.uncertainty=s.decision.predicted_uncertainty;e.decision_source=s.decision.thought;}
    e.causal_clarity=c.causal_clarity;e.stage=s.stage;e.recovery_observed=s.recovery_observed;
    e.recovered=s.recovered;e.functioning=s.functioning;e.consequences=s.consequences;e.elapsed_recovery_ms=s.elapsed_recovery_ms;
    return e;
}
Attribution attribute_outcome(const InterpretedEpisode& e,const OutcomeSignal& s,const InterpretationContext& c){
    validate_outcome(s);
    for(double x:{c.causal_clarity,c.perception_quality,c.rejection_bias,c.prior_causal_control,c.self.efficacy,c.self.control,e.success,e.uncertainty})require_range(x,0,1);
    const double clarity=c.causal_clarity*c.perception_quality;
    // Ambiguous failure can be interpreted through a prior self-schema. An
    // explicit visible obstacle instead supplies strong external evidence.
    const double self_blame=(1-e.success)*c.rejection_bias*(1-c.self.efficacy)*(1-clarity);
    double own=(e.intentional?.08+.9*s.action_feedback*clarity:0)+.7*self_blame;
    double other=1.2*s.other_decision*clarity;
    double environment=3*s.observed_obstacle*clarity;
    double chance=.04+e.uncertainty*(1-clarity)+.08*(1-clarity);
    const double sum=own+other+environment+chance;
    Attribution a;a.self=own/sum;a.other=other/sum;a.environment=environment/sum;a.chance=chance/sum;
    // Control measures perceived correctability/predictability, not success.
    // It can remain high after a understood, self-caused error.
    a.controllability=unit(.15+.6*a.self+.2*clarity+.05*c.prior_causal_control);
    a.confidence=unit(.15+.65*clarity+.2*std::max({s.action_feedback,s.other_decision,s.observed_obstacle}));
    a.validate();return a;
}
Appraisal self_appraisal(const SelfPrediction& p,double importance,double expected_harm,double uncertainty){
    for(double x:{p.control,p.coping,importance,expected_harm,uncertainty})require_range(x,0,1);
    Appraisal a;a.significance=importance;a.control=p.control;a.harm=expected_harm*(1-p.coping);
    a.uncertainty=uncertainty;a.immediacy=.5;return a;
}
double outcome_priority(const OutcomeSignal& s){
    return unit(.50+.30*s.goal_importance+.10*(s.blocked||s.acceptance_observed)+.10*(s.decision.known?s.decision.predicted_uncertainty:0));
}
} // namespace life
