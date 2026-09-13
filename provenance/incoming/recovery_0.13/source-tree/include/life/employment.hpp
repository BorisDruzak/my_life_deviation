#pragma once
#include "life/numeric.hpp"
#include "life/semantic_types.hpp"
#include <vector>
namespace life {
struct EmploymentContract {
    bool active=false;Id organization=0,workplace=0,supervisor=0;unsigned rank=0;
    double hourly=12.5,promise_importance=.7,known_multiplier=1;
    Tick shift_start=9*3600000LL,shift_end=17*3600000LL;
    std::uint64_t source=0,last_request=0;Tick request_expires=0;double requested_effort=0;
    double work_streak=0;Tick break_until=0;std::int64_t created_shift=-1;
    bool in_shift(Tick now)const;
    double obligation(Tick now)const;
    bool receive_request(std::uint64_t event,Id speaker,double strength,Tick now);
    template<class A>void fields(A& a){a(active,organization,workplace,supervisor,rank,hourly,promise_importance,known_multiplier,shift_start,shift_end,source,last_request,request_expires,requested_effort,work_streak,break_until,created_shift);}
};
struct EmployeeContribution {
    Id person=0;double hourly=12.5,role_weight=1,target_hours=5;
    double hours=0,effective_hours=0,total_hours=0,paid=0;
    bool active=true;double accrued_base=0;
    template<class A>void fields(A& a){a(active,accrued_base,person,hourly,role_weight,target_hours,hours,effective_hours,total_hours,paid);}
};
struct PayrollEntry {
    Id person=0;std::int64_t day=0;double hours=0,effective_hours=0,multiplier=1,amount=0;
    template<class A>void fields(A& a){a(person,day,hours,effective_hours,multiplier,amount);}
};
struct Organization {
    Id id=1,place=5,supervisor=1;
    double development=0;unsigned level=0;std::int64_t settled_day=-1;
    std::vector<EmployeeContribution> members;
    std::vector<std::uint64_t> credited;
    std::vector<PayrollEntry> payroll;
    bool contribute(Id person,std::uint64_t action,double hours,double efficiency);
    std::vector<PayrollEntry> settle(std::int64_t day);
    double multiplier()const;
    void validate()const;
    template<class A>void fields(A& a){a(id,place,supervisor,development,level,settled_day,members,credited,payroll);}
};
} // namespace life
