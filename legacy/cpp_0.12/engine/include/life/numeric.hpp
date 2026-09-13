#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace life {
using Tick = std::int64_t; // logical milliseconds
constexpr double hour = 3600.0;
constexpr double day = 86400.0;
double unit(double x);
double signed_unit(double x);
void require_range(double x, double lo, double hi);
double approach(double x, double target, double seconds, double tau);
double saturation(double x, double positive, double negative, double leak, double hours);

struct Biology {
    double metabolism=1, water_rate=1, sleep_need=1, endurance=1, recovery=1, pain_sensitivity=1;
    double leisure_need=1, social_need=1;
    std::array<double,9> sensitivity{1,1,1,1,1,1,1,1,1};
    template<class A> void fields(A& a) { a(metabolism,water_rate,sleep_need,endurance,recovery,pain_sensitivity,leisure_need,social_need,sensitivity); }
};
struct Body {
    double energy=.8, water=.8, sleep=.15, fatigue=.1, damage=0, pain=0, temperature=0, oxygen=0, activation=.1;
    template<class A> void fields(A& a) { a(energy,water,sleep,fatigue,damage,pain,temperature,oxygen,activation); }
};
struct Input {
    double load=0, temperature=0, oxygen=0, pain=0, disease=0, safety=1, activation_effect=0;
    bool asleep=false, conscious=true;
};
enum class Emotion: std::uint8_t { Fear,Anxiety,Anger,Sadness,Joy,Surprise,Disgust,Interest,Shame,Guilt,Count };
constexpr std::size_t emotion_count=10;
using Emotions=std::array<double,emotion_count>;
void advance_body(Body& b, const Biology& p, const Input& in, const Emotions& old_emotions, double seconds);
std::array<double,8> signals(const Body& b,const Biology& p);
double sensation(double signal,double attention_share,double anxiety);
struct Cognitive {
    std::array<double,8> base{.5,.5,.5,.5,.5,.5,.5,.5};
    double learnability=.5, plasticity=.5;
    template<class A> void fields(A& a) { a(base,learnability,plasticity); }
};
struct Capability {
    std::array<double,8> current{};
    double gate=0;
    int context=0, operations=0, depth=0, alternatives=0;
    template<class A> void fields(A& a){a(current,gate,context,operations,depth,alternatives);}
};
Capability capability(const Body& b,const Cognitive& c,bool asleep,bool conscious=true);
struct Appraisal {
    double significance=0,harm=0,immediacy=0,uncertainty=1,control=.5,obstacle=0,blame=0,loss=0;
    double progress=0,positive_error=0,discrepancy=0,novelty=0,comprehension=0,rejection=0,disapproval=0,violation=0,responsibility=0;
    double pleasantness=0,question_usefulness=0;
};
Emotions appraisal_targets(const Appraisal& a,const Body& b);
struct AffectCause {
    std::uint64_t id=0;
    Tick expires=0;
    Emotions target{},value{};
    template<class A> void fields(A& a) { a(id,expires,target,value); }
};
struct Affect {
    std::vector<AffectCause> causes;
    Emotions total{};
    void set(std::uint64_t id,Tick expires,const Emotions& target);
    void advance(Tick now,Tick delta);
    template<class A> void fields(A& a) { a(causes,total); }
};
struct Desire {
    double deficit=.25,refractory=0,value=.15;
    double baseline=.1,accumulation=.009627044174443685,sensitivity=.45,decay=.08664339756999316;
    std::vector<std::uint64_t> applied_results;
    void advance(double seconds,double trigger,double inhibition);
    bool satisfy(std::uint64_t serial,double amount);
    template<class A> void fields(A& a) { a(deficit,refractory,value,baseline,accumulation,sensitivity,decay,applied_results); }
};
double habituation(double old,double engagement,double hours);
double pleasantness(double base,double innate,double acquired,double relation,double context,double consequences,double engagement,double satiation,double rho=.4);
struct Attention {
    std::uint32_t focus=0;
    Tick since=0,ready=0;
    bool select(std::uint32_t candidate,double old_priority,double new_priority,bool emergency,Tick now,const Capability& c,double commitment,double difference);
    template<class A> void fields(A& a) { a(focus,since,ready); }
};
} // namespace life
