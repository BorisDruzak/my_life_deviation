#pragma once
#define LIFE_CAREER_RUNTIME 1
#include "life/numeric.hpp"
#include "life/semantic_types.hpp"
#include <array>
#include <vector>
#include <string>
namespace life {
enum JobKnowledgeField : unsigned {LocationKnown=1,WageKnown=2,ScheduleKnown=4,VacancyKnown=8,QualificationKnown=16};
enum class CareerStage:std::uint8_t {Review,Inquire,Apply,Study,Deferred,Hired};
const char* career_stage_name(CareerStage stage);
struct JobKnowledge {
    Id organization=0,place=0;double hourly=0;Tick shift_start=0,shift_end=0;
    bool vacancy=false;Id required_skill=0;unsigned known=0;
    std::uint64_t revision=1;std::array<std::uint64_t,5> sources{};std::array<Tick,5> times{};
    void validate()const;
    template<class A>void fields(A& a){a(organization,place,hourly,shift_start,shift_end,vacancy,required_skill,known,revision,sources,times);}
};
struct CareerAssessment {
    CareerStage stage=CareerStage::Deferred;double expected_gain=0,uncertainty=1;
    std::string reason="no_known_opportunity";
    template<class A>void fields(A& a){a(stage,expected_gain,uncertainty,reason);}
};
struct CareerQuestion {
    Id organization=0;CareerStage stage=CareerStage::Review;
    std::uint64_t reviewed_revision=0,basis=0;Tick next_review=0;
    CareerAssessment assessment;std::uint64_t reviews=0;
    template<class A>void fields(A& a){a(organization,stage,reviewed_revision,basis,next_review,assessment,reviews);}
};
struct CareerEvidence {
    Tick at=0;Id organization=0;std::uint64_t source=0,action=0;unsigned changed_fields=0;
    std::string kind;double before_wage=0,after_wage=0;
    template<class A>void fields(A& a){a(at,organization,source,action,changed_fields,kind,before_wage,after_wage);}
};
struct CareerMemory {
    bool enabled=false;std::vector<JobKnowledge> jobs;std::vector<CareerQuestion> questions;
    std::uint64_t revision=1,received=0,semantic_changes=0,reopened=0,questions_reviewed=0;
    std::uint64_t inquiries=0,applications=0,hired=0,refused=0,interrupted=0,changed_by_information=0;
    std::uint64_t inquiry_starts=0,application_starts=0,interrupted_applications=0,failed_applications=0;
    std::vector<CareerEvidence> evidence;
    void record(CareerEvidence item);
    Id active_target=0;Tick retry_at=0;std::uint64_t active_basis=0;
    const JobKnowledge* find(Id organization)const;
    JobKnowledge* find(Id organization);
    bool receive(const JobKnowledge& message,std::uint64_t source,Tick now);
    void validate()const;
    template<class A>void fields(A& a){a(enabled,jobs,questions,revision,received,semantic_changes,reopened,questions_reviewed,inquiries,applications,hired,refused,interrupted,changed_by_information,active_target,retry_at,active_basis,inquiry_starts,application_starts,interrupted_applications,failed_applications,evidence);}
};
CareerAssessment assess_career(const JobKnowledge& job,double own_hourly,bool employed,double mastery);
struct CareerPlanView {
    bool enabled=false;CareerStage stage=CareerStage::Deferred;JobKnowledge offer;
    double expected_gain=0;std::uint64_t basis=0;Tick retry_at=0;
    template<class A>void fields(A& a){a(enabled,stage,offer,expected_gain,basis,retry_at);}
};
struct HiringOffice {
    Id organization=0,place=0;unsigned capacity=0;double hourly=12.5;Id required_skill=0;
    Tick shift_start=9*3600000LL,shift_end=17*3600000LL;
    std::string name;
    template<class A>void fields(A& a){a(organization,place,capacity,hourly,required_skill,shift_start,shift_end,name);}
};
} // namespace life
