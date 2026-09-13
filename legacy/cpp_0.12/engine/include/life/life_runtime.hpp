#pragma once
#include "life/projects.hpp"
#include "life/employment.hpp"
#include "life/activity_resources.hpp"
#include <vector>
namespace life {
struct MeetingAgreement {
    std::uint64_t id=0;Id a=0,b=0,place=0;Tick at=0,until=0;bool private_visit=false;
    template<class A>void fields(A& ar){ar(id,a,b,place,at,until,private_visit);}
};
struct RelationshipView {
    Id person=0,last_known_place=0;Tick last_seen=0;
    bool introduced=false,visible=false,attraction_known=false;
    double familiarity=0,care=0,pleasure=0,safety=.5,meeting_acceptance=.65,intimacy_acceptance=.5,attraction=0;
    std::uint64_t basis=0;
    template<class A>void fields(A& a){a(person,last_known_place,last_seen,introduced,visible,attraction_known,familiarity,care,pleasure,safety,meeting_acceptance,intimacy_acceptance,attraction,basis);}
};
struct LifePlanView {
    bool enabled=false;MotivationProfile profile;
    std::vector<PersonalProject> projects;std::vector<KnownProcedure> procedures;
    std::vector<RelationshipView> people;std::vector<MeetingAgreement> meetings;
    EmploymentContract employment;
    std::uint64_t focus_project=0,current_project=0,conversation_project=0;
    double private_satiation=0;bool primary_busy=false;Tick conversation_started=0;
    template<class A>void fields(A& a){a(enabled,profile,projects,procedures,people,meetings,employment,focus_project,current_project,conversation_project,private_satiation,primary_busy,conversation_started);}
};
struct LifeRuntime {
    bool enabled=false,parallel=true;Tick next_payroll=17*3600000LL;
    std::vector<Organization> organizations;std::vector<MeetingAgreement> meetings;
    std::uint64_t introductions=0,meetings_agreed=0,intimacies=0,conversations=0,parallel_started=0,work_requests=0,work_request_refusals=0;
    double parallel_seconds=0,partner_seconds=0;
    template<class A>void fields(A& a){a(enabled,parallel,next_payroll,organizations,meetings,introductions,meetings_agreed,intimacies,conversations,parallel_started,work_requests,work_request_refusals,parallel_seconds,partner_seconds);}
};
} // namespace life
