#include "subjective.hpp"
#include "attention.hpp"
#include "valuation.hpp"
#include "scheduler.hpp"
#include <iomanip>
#include <iostream>
using namespace cog04;
int main(){
 try{
  std::cout<<std::fixed<<std::setprecision(8);
  const double d=(.65-.39)/.65,s=body_signal(d,1),y=sensation(s,1,.2);
  const auto b=fuse(Reading{y,.8,1},Reading{.2,.6,2});
  std::cout<<"deficit="<<d<<" signal="<<s<<" felt="<<y<<" belief="<<b.mean<<" confidence="<<b.confidence<<'\n';
  auto n=norm_response(.9,.95,.8,0,.7,.65,.9);
  std::cout<<"norm_resistance="<<n.resistance<<" shame_target="<<n.shame_target<<" shame_after_8s="<<approach(0,n.shame_target,8,8)<<" guilt_target="<<n.guilt_target<<'\n';
  Ledger salary;salary.impulse(1,0,0,-2);salary.impulse(2,0,720,7.2);
  std::cout<<"salary_plan_30days="<<salary.value(720,cfg::discount_per_hour)<<" normalized="<<squash(salary.value(720,cfg::discount_per_hour))<<'\n';
  std::cout<<"future_relief="<<projected_relief(.1,.8,.1,0)<<" projection_b_0.7="<<projected_relief(.1,.8,.1,.7)<<'\n';
  std::cout<<"candidate_priority="<<priority(.8,.5,.2,.9,.3,.6)<<" context_slots="<<capacity(.5,.5,true)<<" episode_operations="<<operation_budget(.5,true)<<" operations_per_second="<<operation_rate(.5,true)<<'\n';
  BarrierScheduler q(4);for(Id a=0;a<4;++a)q.enqueue(0,200,a,int(a+1));q.run(1,2,true);
  for(auto r:q.receipts())std::cout<<"logical_commit_ms="<<r.at<<" actor="<<r.actor<<" result="<<r.value<<'\n';
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}return 0;
}
