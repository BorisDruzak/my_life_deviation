#pragma once
#include "core.hpp"
#include <map>
namespace cog04 {
// These are the existing internal v0.1 signals, not a new physiology model.
inline double body_signal(double deficit,double sensitivity){
  unit(deficit);nonnegative(sensitivity);return clip(sensitivity*deficit*(1.+.5*deficit));
}
inline double sensation(double signal,double focus_share,double previous_anxiety){
  unit(signal);unit(focus_share);unit(previous_anxiety);
  return clip(signal*(.35+.65*focus_share)*(1.+.20*previous_anxiety));
}
struct Reading { double value{},quality{};Id source{}; };
struct Belief {
  Status status{Status::Unknown};double mean{},confidence{};
  bool fresh_observation{};std::array<Id,2> sources{};
};
struct FusionParameters {double prior_weight{cfg::prior_weight}, conflict_gap{cfg::conflict_gap}, reliable_quality{cfg::reliable_quality};};
inline Belief fuse(std::optional<Reading> sensation_reading,std::optional<Reading> prior,
                   FusionParameters p={}){
  nonnegative(p.prior_weight);unit(p.conflict_gap);unit(p.reliable_quality);
  for(auto& r:{sensation_reading,prior}) if(r){unit(r->value);unit(r->quality);if(!r->source)throw std::invalid_argument("missing provenance");}
  if(sensation_reading&&sensation_reading->quality==0)sensation_reading.reset();
  if(prior&&prior->quality==0)prior.reset();
  if(!sensation_reading&&!prior)return {};
  if(!sensation_reading)return {Status::Supported,prior->value,prior->quality,false,{prior->source,0}};
  if(prior&&sensation_reading->source==prior->source&&
     (sensation_reading->value!=prior->value||sensation_reading->quality!=prior->quality))
    throw std::invalid_argument("same evidence ID has inconsistent payload");
  if(!prior||sensation_reading->source==prior->source)
    return {Status::Supported,sensation_reading->value,sensation_reading->quality,true,{sensation_reading->source,0}};
  auto s=*sensation_reading;auto b=*prior;double diff=std::abs(s.value-b.value);
  if(diff>p.conflict_gap&&s.quality>=p.reliable_quality&&b.quality>=p.reliable_quality)
    return {Status::Conflicting,0,0,true,{s.source,b.source}}; // 0 is not readable as an estimate in this status.
  const double ws=s.quality,wp=p.prior_weight*b.quality;
  return {Status::Supported,(ws*s.value+wp*b.value)/(ws+wp),
          std::max(s.quality,b.quality)*(1.-diff),true,{s.source,b.source}};
}
// The key represents the SAME episode plus a revision of its interpretation.
// Updating an interpretation is NOT counted as a new outcome for Q/W learning.
class BeliefRegister {
  std::map<std::pair<Id,Id>,Belief> cache_;std::size_t updates_{};
public:
  bool update(Id episode,Id interpretation_version,std::optional<Reading>s,std::optional<Reading>p){
    const auto key=std::pair{episode,interpretation_version};
    if(cache_.contains(key))return false;
    cache_.emplace(key,fuse(s,p));++updates_;return true;
  }
  std::size_t updates()const{return updates_;}
};
enum class Band:int {Unknown=-1,Low=0,Medium=1,High=2,Critical=3};
inline Band band(double x,Band previous){
  unit(x);constexpr std::array<double,3> limits{.25,.55,.85};constexpr double hysteresis=cfg::band_hysteresis;
  int i=int(previous);
  if(i<0){i=0;while(i<3&&x>=limits[i])++i;return Band(i);}
  if(i>3)throw std::invalid_argument("invalid band");
  while(i<3&&x>=limits[i]+hysteresis)++i;
  while(i>0&&x<limits[i-1]-hysteresis)--i;
  return Band(i);
}
// This illustrates observation masking; full Q learning retains the normative dose/memory coefficients.
inline double learn(double q,std::optional<double> observed,double alpha){
  finite(q);unit(alpha);if(!observed)return q;finite(*observed);
  if(q< -1||q>1||*observed< -1||*observed>1)throw std::invalid_argument("signed channel outside [-1,1]");
  return q+alpha*(*observed-q);
}
}
