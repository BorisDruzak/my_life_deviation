#pragma once
#include "life/semantic_types.hpp"
#include "life/numeric.hpp"
#include <array>
#include <vector>
#include <cstddef>

namespace life {
enum class GoalKind:std::uint8_t {FindCompany,MeetPerson,SeekIntimacy,WorkShift,LearnSkill,Count};
enum class ProjectStatus:std::uint8_t {Candidate,Active,Waiting,Paused,Completed,Abandoned,Expired};
enum class StepKind:std::uint8_t {Visit,Contact,Introduce,Invite,Attend,SpendTime,Intimacy,Work,Study,Finish};
enum class StepOutcome:std::uint8_t {Success,Refused,Unavailable,Interrupted};
const char* goal_name(GoalKind goal);
const char* project_status_name(ProjectStatus status);
struct ProcedureStep {
    StepKind kind=StepKind::Visit;
    std::uint16_t success=0,refused=0,unavailable=0;
    Tick retry_delay=1800000;
    template<class A>void fields(A& a){a(kind,success,refused,unavailable,retry_delay);}
};
struct KnownProcedure {
    std::uint32_t id=0;GoalKind goal=GoalKind::FindCompany;
    double mastery=0,expected_gain=.5,successes=1,failures=1;
    std::uint64_t source=0;std::vector<ProcedureStep> steps;
    template<class A>void fields(A& a){a(id,goal,mastery,expected_gain,successes,failures,source,steps);}
};
struct GoalCandidate {
    GoalKind kind=GoalKind::FindCompany;Id target=0,place=0;
    double importance=.5,expected_gain=.5,success=.65,remaining_cost=.05;
    Tick not_before=0,deadline=0,delay=0;std::uint64_t basis=0;
    template<class A>void fields(A& a){a(kind,target,place,importance,expected_gain,success,remaining_cost,not_before,deadline,delay,basis);}
};
struct PersonalProject {
    std::uint64_t id=0;GoalCandidate goal;std::uint32_t procedure=0;
    ProjectStatus status=ProjectStatus::Candidate;std::uint16_t step=0;
    Tick created=0,updated=0,retry_at=0;std::uint32_t failures=0;
    std::uint64_t appointment=0;std::vector<std::uint64_t> observations;
    Id searched_place=0;Tick searched_at=0;
    bool live()const;
    template<class A>void fields(A& a){a(id,goal,procedure,status,step,created,updated,retry_at,failures,appointment,observations,searched_place,searched_at);}
};
struct NeedThreshold {
    double notice=.25,release=.20,strong=.65;
    double activation(double perceived,bool was_active=false)const;
    void validate()const;
    template<class A>void fields(A& a){a(notice,release,strong);}
};
struct MotivationProfile {
    std::array<NeedThreshold,9> thresholds{};
    double discount_per_hour=.035,switch_margin=.06,romantic_interest=.6;
    bool forecast_satiation=true;
    template<class A>void fields(A& a){a(thresholds,discount_per_hour,switch_margin,romantic_interest,forecast_satiation);}
};
struct ProjectMemory {
    bool enabled=false;
    MotivationProfile profile;
    std::vector<KnownProcedure> procedures;
    std::vector<PersonalProject> projects;
    std::uint64_t next_id=1,revision=1,created=0,completed=0,abandoned=0,pauses=0,resumes=0;
    Tick last_generation=-30000;
    const KnownProcedure* known(std::uint32_t id)const;
    PersonalProject* find(std::uint64_t id);
    const PersonalProject* find(std::uint64_t id)const;
    std::uint64_t propose(const GoalCandidate& goal,std::uint32_t procedure,Tick now);
    bool activate(std::uint64_t id,Tick now);
    bool pause(std::uint64_t id,Tick now,Tick retry);
    bool abandon(std::uint64_t id,Tick now);
    bool observe(std::uint64_t id,StepOutcome outcome,std::uint64_t event,Tick now);
    void review(Tick now);
    void validate(Tick now)const;
    template<class A>void fields(A& a){a(enabled,profile,procedures,projects,next_id,revision,created,completed,abandoned,pauses,resumes,last_generation);}
};
// A single subjective estimate of the remaining chain; never credits an actual reward.
double continuation_value(const GoalCandidate& goal,double discount_per_hour);
double predicted_satiated_gain(double expected_gain,double perceived_satiation);
KnownProcedure familiar_procedure(GoalKind goal,std::uint64_t source);
} // namespace life
