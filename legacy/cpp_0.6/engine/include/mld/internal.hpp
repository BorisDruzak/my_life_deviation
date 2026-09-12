#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <limits>
namespace mld {
    using Time = std::int64_t; // integer milliseconds
    using Id = std::uint32_t;
    constexpr Id none = std::numeric_limits<Id>::max();
#define MLD_FIELDS(...) template<class A> void io(A& archive) { archive(__VA_ARGS__); }
    double clamp(double x, double lo=0, double hi=1);
    void require(double x, double lo=0, double hi=1);
    double relax(double x,double target,double seconds,double tau);
    double reservoir(double x,double a,double b,double leak,double hours);
    struct Body {
        double energy=.8,water=.8,sleep=.15,fatigue=.1,damage=0,pain=0,temp=0,oxygen=0,activation=.1;
        MLD_FIELDS(energy,water,sleep,fatigue,damage,pain,temp,oxygen,activation)
    };
    struct Biology {
        double energy_use=1,water_use=1,sleep_need=1,endurance=1,recovery=1,pain_sensitivity=1,social_need=1,leisure_need=1;
        std::array<double,8> intellect{.5,.5,.5,.5,.5,.5,.5,.5};
        double learning=.5,plasticity=.5;
        MLD_FIELDS(energy_use,water_use,sleep_need,endurance,recovery,pain_sensitivity,social_need,leisure_need,intellect,learning,plasticity)
    };
    struct PhysicalInput {double load=0,temp=0,oxygen=0,pain=0,disease=0,safety=.5;bool asleep=false,conscious=true;};
    struct Appraisal {
        double significance=0,harm=0,near=0,uncertain=1,control=.5,obstacle=0,blame=0,loss=0,progress=0,gain=0,surprise=0,novelty=0,understand=.5,disgust=0,condemn=0,violation=0,responsibility=0,pleasant=0,question=0;
    };
    using Emotions=std::array<double,10>; // fear anxiety anger sadness joy surprise disgust interest shame guilt
    Body advance_body(const Body&,const Biology&,const PhysicalInput&,const Emotions&,double seconds);
    Emotions emotion_targets(const Appraisal&);
    std::array<double,8> capacities(const Body&,const Biology&,bool asleep=false,bool conscious=true);
    std::array<double,8> body_signals(const Body&);
    int context_slots(const std::array<double,8>&,bool gate=true);
    int operations(const std::array<double,8>&,bool gate=true);
    struct Desire {
        double deficit=.25,satiation=0,value=.15,background=.1,sensitivity=.45,rate=.009627044174443685;
        MLD_FIELDS(deficit,satiation,value,background,sensitivity,rate)
    };
    Desire advance_desire(Desire d,double trigger,double inhibition,double seconds);
    void satisfy(Desire& d,double fullness);
    double pleasure(double basic,double innate,double acquired,double relationship,double context,double engagement,double saturation,double sensitivity,double consequences);
    struct Stats {
        double mean=0,variance=.25,count=0;
        MLD_FIELDS(mean,variance,count)
    };
    void learn_association(Stats&,std::optional<double> result,double learning,double quality,double memory,double dose,bool conscious=true);
    struct Feature {Id id=0;double exposure=0;MLD_FIELDS(id,exposure)};
    struct Weight {Id feature=0;std::uint8_t channel=0;double value=0;Time updated=0,active=0;MLD_FIELDS(feature,channel,value,updated,active)};
    struct Reactions {
        std::vector<Weight> weights;
        std::uint64_t last_episode=0;
        std::array<double,6> effects(std::vector<Feature>,Time now) const;
        std::size_t learn(std::uint64_t episode,std::vector<Feature>,const std::array<std::optional<double>,6>& primary,double plasticity,double dose,Time now,std::size_t address_properties=0);
        MLD_FIELDS(weights,last_episode)
    };
    enum class Truth:std::uint8_t { Unknown,True,False,Conflict };
    struct Fact {Id predicate=0,object=0;Truth truth=Truth::Unknown;double confidence=0;std::uint64_t source=0;MLD_FIELDS(predicate,object,truth,confidence,source)};
    struct Condition {Id predicate=0;Truth required=Truth::True;};
    struct Rule {Id id=0;std::vector<Condition> conditions;Id conclusion=0;double mastery=1;};
    std::optional<Fact> infer(const Rule&,const std::vector<Fact>&,Id object,int& budget);
    double moral_cost(double acceptance,double severity);
    double expected_sanction(double detect,double qualify,double react,double severity);
    struct DependenceInput {double importance;std::optional<double> contribution,substitution;};
    struct Dependence {std::optional<double> value;double coverage;};
    Dependence dependence(const std::vector<DependenceInput>&);
}
