#include "life/employment.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace life {
bool EmploymentContract::in_shift(Tick now)const{
    if(now<0)throw std::invalid_argument("negative employment time");
    return active&&(now/86400000)%7<5&&now%86400000>=shift_start&&now%86400000<shift_end;
}
double EmploymentContract::obligation(Tick now)const{
    require_range(promise_importance,0,1);
    if(!in_shift(now))return 0;
    return unit(.35+.45*promise_importance+(now<request_expires?.2*requested_effort:0));
}
bool EmploymentContract::receive_request(std::uint64_t event,Id speaker,double strength,Tick now){
    require_range(strength,0,1);if(!event||now<0)throw std::invalid_argument("work request source/time");
    if(!active||speaker!=supervisor||event<=last_request)return false;
    last_request=event;requested_effort=strength;request_expires=now+1800000;return true;
}
bool Organization::contribute(Id person,std::uint64_t action,double hours,double efficiency){
    require_range(hours,0,8);require_range(efficiency,0,1);
    if(!action)throw std::invalid_argument("work needs an action source");
    auto it=std::find_if(members.begin(),members.end(),[&](const auto& m){return m.person==person;});
    if(it==members.end())throw std::invalid_argument("not an employee");
    if(std::binary_search(credited.begin(),credited.end(),action))return false;
    credited.insert(std::lower_bound(credited.begin(),credited.end(),action),action);
    it->accrued_base+=hours*it->hourly;it->hours+=hours;it->effective_hours+=hours*efficiency;it->total_hours+=hours;return true;
}
std::vector<PayrollEntry> Organization::settle(std::int64_t day){
    if(day<0)throw std::invalid_argument("negative payroll day");
    if(day<=settled_day)return {};
    std::vector<PayrollEntry> result;
    const double old_multiplier=multiplier();double least=1,weighted=0,total_weight=0;
    for(auto& m:members){
        if(!m.active&&m.hours==0)continue;
        const double normalized=unit(m.effective_hours/m.target_hours);
        if(m.active){least=std::min(least,normalized);weighted+=normalized*m.role_weight;total_weight+=m.role_weight;}
        PayrollEntry p{m.person,day,m.hours,m.effective_hours,old_multiplier,m.accrued_base*old_multiplier};
        result.push_back(p);payroll.push_back(p);m.paid+=p.amount;m.hours=m.effective_hours=m.accrued_base=0;
    }
    if(total_weight>0)development+=least*(.25+.75*weighted/total_weight);
    // The newly attained organizational level changes FUTURE rates, not money
    // that was already earned under yesterday's known agreement.
    level=unsigned(std::floor(development/5+1e-12));settled_day=day;return result;
}
double Organization::multiplier()const{return 1+.08*level;}
void Organization::validate()const{
    if(!id||!place||!supervisor||settled_day< -1)throw std::runtime_error("organization identity/time");
    require_range(development,0,1000000);
    if(level!=unsigned(std::floor(development/5+1e-12)))throw std::runtime_error("development level mismatch");
    std::set<Id> seen;
    for(const auto& m:members){
        if(!m.person||!seen.insert(m.person).second)throw std::runtime_error("duplicate employee");
        require_range(m.hourly,.001,1e6);require_range(m.role_weight,.001,10);require_range(m.target_hours,.001,8);
        for(double x:{m.hours,m.effective_hours,m.total_hours,m.paid,m.accrued_base})require_range(x,0,1e12);
        if(m.effective_hours>m.hours+1e-9)throw std::runtime_error("phantom work effort");
    }
    if(!std::is_sorted(credited.begin(),credited.end())||std::adjacent_find(credited.begin(),credited.end())!=credited.end())throw std::runtime_error("duplicate work credit");
    for(const auto& p:payroll){if(!seen.contains(p.person)||p.day<0||p.day>settled_day)throw std::runtime_error("payroll identity/time");for(double x:{p.hours,p.effective_hours,p.multiplier,p.amount})require_range(x,0,1e12);}
}
}
