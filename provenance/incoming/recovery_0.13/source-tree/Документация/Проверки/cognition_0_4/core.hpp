#pragma once
#include "profile.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <vector>
namespace cog04 {
using Id=std::uint64_t;
using Ms=std::int64_t;
enum class Status { Unknown, Supported, Refuted, Conflicting };
inline void finite(double x){if(!std::isfinite(x))throw std::invalid_argument("non-finite value");}
inline void unit(double x){finite(x);if(x<0||x>1)throw std::invalid_argument("outside [0,1]");}
inline void nonnegative(double x){finite(x);if(x<0)throw std::invalid_argument("negative value");}
inline double clip(double x){finite(x);return std::clamp(x,0.,1.);}
inline double squash(double x){finite(x);return x/(1.+std::abs(x));}
inline double approach(double old,double target,double seconds,double tau){
  finite(old);finite(target);nonnegative(seconds);finite(tau);
  if(tau<=0)throw std::invalid_argument("nonpositive time constant");
  return old+(target-old)*(-std::expm1(-seconds/tau));
}
inline Status conjunction(std::span<const Status> premises){
  if(premises.empty()||premises.size()>6)throw std::invalid_argument("rule needs 1..6 premises");
  if(std::find(premises.begin(),premises.end(),Status::Conflicting)!=premises.end())return Status::Conflicting;
  if(std::find(premises.begin(),premises.end(),Status::Refuted)!=premises.end())return Status::Refuted;
  if(std::find(premises.begin(),premises.end(),Status::Unknown)!=premises.end())return Status::Unknown;
  return Status::Supported;
}
}
