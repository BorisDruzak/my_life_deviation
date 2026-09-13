#pragma once
#include "life/numeric.hpp"
#include <cstdint>
namespace life {
using Id=std::uint32_t;
enum class Truth:std::uint8_t {Unknown,Confirmed,Refuted,Conflicting};
struct Estimate {
    double mean=0,variance=.25,count=0;
    void observe(double y,double learning,double quality,double dose,double memory,bool conscious);
    double confidence(double similarity=1,double multiplier=1)const;
    template<class A> void fields(A& a){a(mean,variance,count);}
};
}
