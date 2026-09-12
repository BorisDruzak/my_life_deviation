#pragma once
#include "life/numeric.hpp"
#include "life/semantic_types.hpp"
#include <array>
namespace life {
enum class Method:std::uint8_t;
enum class ActivityResource:std::uint8_t {Locomotion,Hands,Vision,Hearing,Speech,Attention,Count};
struct ActivityResources {
    std::array<double,6> load{};bool exclusive=false;
    template<class A>void fields(A& a){a(load,exclusive);}
};
struct ParallelFit {bool allowed=false;double efficiency=0;};
ActivityResources activity_resources(Method method,bool travel=false,bool asleep=false);
ParallelFit combine_resources(const ActivityResources& primary,const ActivityResources& secondary);
struct ParallelConversation {
    std::uint64_t id=0,project=0;Id partner=0;Tick started=0,ends=0;
    double seconds=0,quality_seconds=0;
    template<class A>void fields(A& a){a(id,project,partner,started,ends,seconds,quality_seconds);}
};
} // namespace life
