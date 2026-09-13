#pragma once
#include "life/mind.hpp"
#include "life/experience.hpp"
#include <functional>
#include <memory>
#include <span>

namespace life {
enum class TopicKind:std::uint8_t {Internal,External,Memory,Thought,Project};
enum class ThoughtKind:std::uint8_t {Notice,Interpret,Recall,Forecast,Compare,Intent,Question,Reply,Stale,Pause,Resume,InterpretOutcome,Attribute,SelfUpdate,Recovery,Count};
enum class Origin:std::uint8_t {Observed,Reported,Inferred,Imagined};
enum class Operation:std::uint8_t {None,Interpret,Recall,Forecast,Compare,Commit,Recognize,Reply,SocialReply,SocialObserve,InterpretOutcome,AttributeOutcome,RecallCareerFacts,CompareCareerFacts};
struct SubjectiveNeed {
    Truth status=Truth::Unknown;double mean=0,confidence=0,prior=0,prior_quality=0;
    double observation=0,quality=0;std::uint64_t source=0,prior_source=0;Tick at=0;
    int band=-1;
    template<class A> void fields(A& a){a(status,mean,confidence,prior,prior_quality,observation,quality,source,prior_source,at,band);}
};
struct AttentionTopic {
    std::uint64_t id=0;TopicKind kind=TopicKind::Internal;std::uint8_t metric=0;Id person=0;
    double priority=0,urgency=0;Tick created=0,expires=0;std::uint64_t revision=0,basis=0;bool emergency=false;
    template<class A> void fields(A& a){a(id,kind,metric,person,priority,urgency,created,expires,revision,basis,emergency);}
};
struct ContextItem {
    std::uint64_t id=0,basis=0;ThoughtKind kind=ThoughtKind::Notice;std::uint8_t metric=0;
    Id person=0;double value=0,confidence=0;Truth status=Truth::Unknown;
    template<class A> void fields(A& a){a(id,basis,kind,metric,person,value,confidence,status);}
};
struct Thought {
    std::uint64_t id=0,episode=0,focus=0,basis=0,own_revision=0;
    Id actor=0,person=0;Tick started=0,time=0;ThoughtKind kind=ThoughtKind::Notice;Origin origin=Origin::Inferred;
    Truth status=Truth::Unknown;std::uint8_t metric=0;Method method=Method::Idle;
    double value=0,confidence=0,observed=0,prior=0;int spent=0,slots=0;std::string detail;
    // Derived trace details: not read by simulation and not serialized into state.
    Id debug_destination=0,debug_object=0;Interaction debug_interaction=Interaction::FriendlyTouch;double debug_pleasure=0,debug_acceptance=0,debug_status_gain=0;std::uint64_t debug_knowledge_source=0;double debug_moral=0,debug_risk=0,debug_resource=0,debug_time=0;
    template<class A> void fields(A& a){a(id,episode,focus,basis,own_revision,actor,person,started,time,kind,origin,status,metric,method,value,confidence,observed,prior,spent,slots,detail);}
};
// Sensor bookkeeping is not working memory. It contains no unseen mental state.
struct Percept {
    Id token=0;Tick since=0,last=0;std::uint64_t exposure_id=0;
    double evidence=0,threshold=0,feature_work=0;bool noticed=false,recognized=false;
    template<class A> void fields(A& a){a(token,since,last,exposure_id,evidence,threshold,feature_work,noticed,recognized);}
};
enum class ReplyMessage:std::uint8_t {Busy,Declined,Later,NotHeard,Accepted};
struct SocialObservation {
    std::uint64_t event=0;Id person=0;ReplyMessage message=ReplyMessage::Declined;Tick at=0;
    template<class A> void fields(A& a){a(event,person,message,at);}
};
struct SocialExpectation {
    Id person=0;double accepted=1,refused=1,unpleasantness=0;Tick last_reply=0,next_opportunity=0;
    std::uint64_t last_event=0;ReplyMessage message=ReplyMessage::NotHeard;
    template<class A> void fields(A& a){a(person,accepted,refused,unpleasantness,last_reply,next_opportunity,last_event,message);}
};
struct CognitiveState {
    JobKnowledge career_input;CareerAssessment career_assessment;double career_own_hourly=0,career_mastery=0;bool career_employed=false;
    std::vector<OutcomeSignal> outcome_inbox;
    OutcomeSignal self_input;
    InterpretedEpisode self_interpreted;
    std::array<RecoveryTrace,self_cfg::recovery_capacity> recoveries{};
    std::vector<DecisionExperience> decision_experiences;
    std::uint64_t outcome_published=0,outcome_dropped=0,self_integrations=0,recovery_integrations=0;
    std::uint64_t observed_functioning=0,observed_activities=0;
    std::uint64_t retained_decisions=0,executed_decisions=0,fruitless_decisions=0;

    std::array<SubjectiveNeed,metric_count> needs{};
    std::array<Tick,9> no_continuation_until{}; // own failed consideration, not a physical prohibition
    std::vector<AttentionTopic> candidates;
    std::vector<ContextItem> context;
    std::vector<Percept> percepts;
    std::vector<SocialObservation> inbox;
    std::vector<SocialExpectation> contacts;
    std::vector<Thought> recent; // Only diagnostics; not used as an unbounded hidden reasoning stack.
    std::array<int,9> last_signal_band{-1,-1,-1,-1,-1,-1,-1,-1,-1};
    std::uint64_t episode=0,next_thought=1,situation_version=1,captured_mind=0,captured_situation=0;
    std::uint64_t observation_sequence=1,focus=0,focus_basis=0;
    TopicKind focus_kind=TopicKind::Internal;std::uint8_t focus_metric=0;Id focus_person=0;
    bool emergency=false,active=false;Operation operation=Operation::None;
    Tick review_at=0,focus_since=0,rebuild_until=0,op_started=0,op_last=0,due=0;
    double focus_priority=0,work=0,rate=0,rejection_bias=.35;
    int budget=0,spent=0,peak_slots=0,alternatives=0;std::uint32_t cursor=0;
    InteractionObservation social_input;
    PersonalView snapshot;std::vector<PlanOption> options;Decision current,best;
    std::uint64_t current_thought=0,best_thought=0,project_id=0;bool project_paused=false;
    std::array<std::uint64_t,std::size_t(ThoughtKind::Count)> counts{};
    std::uint64_t operation_starts=0,completed_ops=0,stale_ops=0,focus_switches=0,pressure_events=0;
    template<class A> void fields(A& a){a(career_input,career_assessment,career_own_hourly,career_mastery,career_employed,retained_decisions,executed_decisions,fruitless_decisions,outcome_inbox,self_input,self_interpreted,recoveries,decision_experiences,outcome_published,outcome_dropped,self_integrations,recovery_integrations,observed_functioning,observed_activities,needs,no_continuation_until,candidates,context,percepts,inbox,contacts,recent,last_signal_band,episode,next_thought,situation_version,captured_mind,captured_situation,observation_sequence,focus,focus_basis,focus_kind,focus_metric,focus_person,emergency,active,operation,review_at,focus_since,rebuild_until,op_started,op_last,due,focus_priority,work,rate,rejection_bias,budget,spent,peak_slots,alternatives,cursor,social_input,snapshot,options,current,best,current_thought,best_thought,project_id,project_paused,counts,operation_starts,completed_ops,stale_ops,focus_switches,pressure_events);}
};
const char* thought_kind_name(ThoughtKind kind);
const char* metric_name(std::uint8_t metric);
std::string thought_text(const Thought& t);
std::string thought_json(const Thought& t);
std::vector<AttentionTopic> bounded_topics(std::span<const AttentionTopic> input,Tick now,std::uint64_t focus);
SubjectiveNeed interpret_need(const SubjectiveNeed& previous,double observation,double quality,std::uint64_t source,Tick now);
void validate_cognition(const CognitiveState& c,Tick now);
// Persistent worker pool. Workers receive only immutable PersonalView copies; no World pointer.
class ForecastWorkers {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    explicit ForecastWorkers(unsigned count);~ForecastWorkers();
    std::vector<Decision> evaluate(const std::vector<std::pair<PersonalView,PlanOption>>& jobs,std::size_t chunk);
};
} // namespace life
