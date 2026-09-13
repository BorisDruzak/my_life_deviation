#pragma once
#include "core.hpp"
#include <map>
namespace cog04 {
struct NormResponse {double resistance{},shame_target{},guilt_target{};};
inline NormResponse norm_response(double acceptance,double confidence,double severity,double exception,
                                  double significance,double expected_devaluation,double responsibility){
  for(auto x:{acceptance,confidence,severity,exception,significance,expected_devaluation,responsibility})unit(x);
  const double resistance=acceptance*confidence*severity*(1-exception);
  return {resistance,significance*expected_devaluation,significance*resistance*responsibility};
}
inline double sanction(double detected,double classified_given_detection,double reaction_given_classification,double severity){
  for(auto x:{detected,classified_given_detection,reaction_given_classification,severity})unit(x);
  return detected*classified_given_detection*reaction_given_classification*severity;
}
struct NormCost{Id equivalence_group;double resistance;};
inline double moral_cost(std::span<const NormCost> costs){
  std::map<Id,double> groups;for(auto c:costs){unit(c.resistance);groups[c.equivalence_group]=std::max(groups[c.equivalence_group],c.resistance);}
  double total=0;for(auto [id,x]:groups){(void)id;total+=x;}return clip(total);
}
// One coordinate of a subjective discomfort profile. Not a replacement for body dynamics.
inline double phi(double deficit){unit(deficit);return deficit+cfg::discomfort_quadratic*deficit*deficit;}
inline double projected_relief(double now,double future_without,double future_with,double projection){
  unit(now);unit(future_without);unit(future_with);unit(projection);
  if(future_with>future_without)throw std::invalid_argument("relief helper requires a non-harmful outcome; use signed ledger for harm");
  const double future=phi(future_without)-phi(future_with);
  const double current=phi(now)-phi(std::min(now,future_with));
  return (1-projection)*future+projection*current;
}
struct Branch{double probability{},value{};};
inline double expected(std::span<const Branch> branches){
  if(branches.empty())throw std::invalid_argument("missing branches");
  double mass=0,value=0;for(auto b:branches){unit(b.probability);finite(b.value);mass+=b.probability;value+=b.probability*b.value;}
  if(std::abs(mass-1)>1e-12)throw std::invalid_argument("branches must include residual/unknown outcome");
  return value;
}
struct Effect{Id group{},branch{};double begin{},end{},value{};bool flow{};};
class Ledger {
  std::vector<Effect> effects_;
  void add(Effect e){
    nonnegative(e.begin);nonnegative(e.end);finite(e.value);
    if(e.end<e.begin||(e.flow&&e.end==e.begin))throw std::invalid_argument("invalid effect interval");
    for(const auto&old:effects_)if(old.group==e.group&&old.branch==e.branch){
      if(old.flow!=e.flow)throw std::invalid_argument("same harm/reward uses two valuation representations");
      const bool overlap=e.flow?std::max(old.begin,e.begin)<std::min(old.end,e.end):old.begin==e.begin;
      if(overlap)throw std::invalid_argument("double-counted effect");
    }
    effects_.push_back(e);
  }
public:
  void impulse(Id g,Id branch,double hour,double value){add({g,branch,hour,hour,value,false});}
  void flow(Id g,Id branch,double begin,double end,double value_per_hour){add({g,branch,begin,end,value_per_hour,true});}
  double value(double horizon_hours,double discount_per_hour)const{
    nonnegative(horizon_hours);nonnegative(discount_per_hour);double sum=0;
    for(const auto&e:effects_){
      if(e.end>horizon_hours)throw std::invalid_argument("outcome beyond common horizon; extend horizon or provide a known terminal model");
      if(!e.flow)sum+=std::exp(-discount_per_hour*e.begin)*e.value;
      else if(discount_per_hour==0)sum+=(e.end-e.begin)*e.value;
      else sum+=e.value*std::exp(-discount_per_hour*e.begin)*(-std::expm1(-discount_per_hour*(e.end-e.begin)))/discount_per_hour;
    }
    finite(sum);return sum;
  }
};
inline std::size_t best_plan(std::span<const double> values){
  if(values.empty())throw std::invalid_argument("no feasible known plan");
  for(auto v:values)finite(v);
  return std::size_t(std::max_element(values.begin(),values.end())-values.begin());
}
}
