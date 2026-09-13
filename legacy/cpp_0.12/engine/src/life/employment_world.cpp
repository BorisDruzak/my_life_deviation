#include "life/world.hpp"
#include <algorithm>
#include <stdexcept>
namespace life {
void World::employment_credit(Actor& a){
    if(!state_.life.enabled||!a.employment.active||a.action.method!=Method::Work||a.action.phase!=Phase::Running||a.action.blocked)return;
    auto it=std::find_if(state_.life.organizations.begin(),state_.life.organizations.end(),[&](const auto& o){return o.id==a.employment.organization;});
    if(it==state_.life.organizations.end())throw std::logic_error("missing employment organization");
    const double seconds=double(state_.now-a.action.segment_start)/1000.;
    if(seconds<=0)return;
    const double hours=seconds/3600.;const double efficiency=unit(a.action.effort_seconds/seconds);
    if(it->contribute(a.id,a.action.id,hours,efficiency))a.employment.work_streak+=seconds;
}
void World::employment_tick(){
    auto& s=state_;if(!s.life.enabled||s.now<s.life.next_payroll)return;
    const Tick day=s.life.next_payroll/86400000;
    // Close partial work before payroll. Action IDs make credit exactly-once.
    for(auto& a:s.actors){
        if(a.action.method==Method::Work&&a.action.phase==Phase::Running){physical_until(a,s.now);cancel(a);}
        auto& memory=a.mind.projects;
        for(auto& p:memory.projects)if(p.live()&&p.goal.kind==GoalKind::WorkShift&&p.goal.deadline<=s.now){
            double worked=0,target=5;
            for(const auto& org:s.life.organizations)for(const auto& member:org.members)if(member.person==a.id){worked=member.hours;target=member.target_hours;}
            if(worked+1e-9>=target)memory.observe(p.id,StepOutcome::Success,s.next_id++,s.now);
            else memory.abandon(p.id,s.now);
        }
    }
    for(auto& organization:s.life.organizations){
        for(const auto& pay:organization.settle(day)){
            auto& a=s.actors.at(pay.person-1);a.money+=pay.amount;a.economy.earned+=pay.amount;s.ledger.wages+=pay.amount;
            // A payslip is an explicit personal information channel.
            a.mind.believed_money=a.money;if(a.employment.organization==organization.id){a.employment.known_multiplier=organization.multiplier();a.economy.wage_known=a.employment.hourly*organization.multiplier();}++a.mind.version;
            if(logger_)logger_({s.now,a.id,s.next_id++,Method::Work,"payroll","paid_observed_payslip",a.place});
            else ++s.next_id;
        }
    }
    s.life.next_payroll+=86400000;
}
}
