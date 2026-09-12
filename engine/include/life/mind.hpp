#pragma once
#include "life/numeric.hpp"
#include "life/seed.hpp"
#include "life/social.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace life {
enum class Method:std::uint8_t {Idle,Eat,Drink,Sleep,Rest,Leisure,Talk,PrivateIntimacy,AcquireFood,TakeFood,Work,Inspect,Shelter,Social,UseObject,Study,Count};
constexpr std::size_t method_count=std::size_t(Method::Count);
enum class Metric:std::uint8_t {Food,Water,Sleep,Rest,Leisure,Social,Desire,Money,Safety,Care,Pleasantness,Count};
constexpr std::size_t metric_count=std::size_t(Metric::Count);
using Outcomes=std::array<double,metric_count>;
using Channels=std::array<double,6>; // pleasantness, leisure, social, activation, esteem, contextual desire
const char* method_name(Method m);
struct MethodSpec { std::uint32_t seconds; double load,pleasure; };
const MethodSpec& method_spec(Method m);
std::string catalogue_hash();
struct Feature {
    Id id=0;std::uint64_t source=0;double exposure=0,salience=0;
    bool operator==(const Feature&)const=default;
    template<class A> void fields(A& a){a(id,source,exposure,salience);}
};
struct Episode {
    std::uint64_t id=0;Method method=Method::Idle;Tick time=0;
    double quality=1,dose=1;bool conscious=true;
    Outcomes outcome{};std::uint16_t observed=0;
    Channels primary{};std::uint8_t primary_observed=0;
    std::vector<Feature> features;
    template<class A> void fields(A& a){a(id,method,time,quality,dose,conscious,outcome,observed,primary,primary_observed,features);}
};
struct Weight {
    Id feature=0;std::uint8_t channel=0;double value=0;Tick updated=0,activated=0;
    template<class A> void fields(A& a){a(feature,channel,value,updated,activated);}
};
struct PreparedEffects {
    struct Link {std::size_t index;Id feature;double exposure;};
    std::array<std::vector<Link>,6> channels;
};
struct Learning {
    std::array<std::array<Estimate,metric_count>,method_count> q{};
    std::vector<Weight> weights;
    std::vector<std::uint64_t> processed;
    std::uint64_t episodes=0;
    std::uint32_t address_slots=0;
    std::uint32_t last_considered=0,last_created=0;
    bool observe(const Episode& e,const Cognitive& c,double memory);
    Channels effects(const std::vector<Feature>& features,Tick now)const;
    PreparedEffects prepare(const std::vector<Feature>& features)const;
    Channels prepared_effects(const PreparedEffects& plan,Tick now)const;
    template<class A> void fields(A& a){a(q,weights,processed,episodes,address_slots,last_considered,last_created);}
};
struct Budget {
    int operations=0,slots=0,used=0,held=0,peak=0;
    explicit Budget(const Capability& c):operations(c.operations),slots(c.context){}
    bool pay(int n=1);bool hold(int n=1);void release(int n=1);
};
struct CausalSample {
    std::uint64_t id=0;bool result_observed=false,factor_observed=false,present=false,ordered=false;
    double result=0,quality=1;
    std::vector<Id> other_conditions;
};
struct CausalHypothesis {
    double effect=0,variance=.25,weight=0;
    std::vector<std::uint64_t> used;
    bool compare(const CausalSample& yes,const CausalSample& no,double learning,double causal,int knowledge_depth,Budget& budget);
    double justification()const;
    template<class A> void fields(A& a){a(effect,variance,weight,used);}
};
struct Rule {
    Id id=0,topic=0;std::uint8_t level=1;std::vector<Id> prerequisites;
};
struct Knowledge {
    std::vector<std::pair<Id,double>> mastery;
    double get(Id id)const;
    bool usable(const Rule& r,const std::vector<Rule>& catalogue)const;
    void learn(const Rule& r,const std::vector<Rule>& catalogue,double learning,double causal,double quality,double dose);
    int depth(Id topic,const std::vector<Rule>& catalogue)const;
    template<class A> void fields(A& a){a(mastery);}
};
struct Evidence {
    std::uint64_t delivery=0,known_root=0;
    double result=.5,quality=1;
    template<class A> void fields(A& a){a(delivery,known_root,result,quality);}
};
struct Trust {
    double prior=.5,strength=2;
    std::vector<Evidence> evidence;
    bool receive(Evidence e);void revise(std::uint64_t known_root,double result,double quality);double expectation()const;
    template<class A> void fields(A& a){a(prior,strength,evidence);}
};
struct Relation {
    Id person=0;double care=0,familiarity=0;Tick last_contact=0;
    std::uint8_t role=0;std::uint64_t origin=0;
    std::array<Trust,4> trust; // honesty, competence, obligations, safety
    template<class A> void fields(A& a){a(person,care,familiarity,last_contact,role,origin,trust);}
};
struct GoalDependency { double importance=0;std::optional<double> contribution,replacement; };
struct Dependency { std::optional<double> value;double coverage=0; };
Dependency dependency(const std::vector<GoalDependency>& goals);
double sanction(double detected,double classified,double reacted,double severity);
double revise_norm(double old,double target,double plasticity,double quality,double dose);
struct Aspect { Id group=0;double weight=0,violation=0; };
double moral_cost(const std::vector<Aspect>& aspects);

struct KnownEdge {
    Id from=0,to=0;std::uint32_t seconds=0;Truth open=Truth::Confirmed;
    template<class A> void fields(A& a){a(from,to,seconds,open);}
};
struct KnownPlace {
    Id id=0;std::uint32_t services=0;Truth food=Truth::Unknown;Tick retry_at=0;double price=1;std::array<Tick,method_count> retry_method{};
    int job_level=-1,recreation=-1;double wage=0;
    template<class A> void fields(A& a){a(id,services,food,retry_at,price,retry_method,job_level,recreation,wage);}
};
constexpr std::uint32_t service(Method m){return 1u<<std::uint8_t(m);}
struct Decision {
    Method method=Method::Idle;Id place=0,partner=0;std::vector<Id> path;
    double score=0,moral=0,risk=0,resource_cost=0,time_cost=0;
    int operations=0,peak_slots=0,alternatives=0;
    Interaction interaction=Interaction::FriendlyTouch;Id object=0;SocialEvaluation social_evaluation;
    template<class A> void fields(A& a){a(method,place,partner,path,score,moral,risk,resource_cost,time_cost,operations,peak_slots,alternatives,interaction,object,social_evaluation);}
};
struct PersonalView {
    Id self=0,place=0,home=0;
    Tick now=0;
    std::uint64_t episode=0;
    Capability capability;
    Outcomes need{};
    double money=0;int food=0;
    std::array<bool,method_count> known{};
    std::array<Outcomes,method_count> forecasts{};
    std::array<double,method_count> norms{},risks{};
    std::vector<KnownPlace> places;
    std::vector<KnownEdge> map;
    std::vector<Id> perceived_people;
    SocialView social;EconomyView economy;int focus_metric=-1;
    template<class A> void fields(A& a){a(self,place,home,now,episode,capability,need,money,food,known,forecasts,norms,risks,places,map,perceived_people,social,economy,focus_metric);}
};
struct PlanOption {
    Method method=Method::Idle; Id place=0,partner=0; double salience=0;Interaction interaction=Interaction::FriendlyTouch;Id object=0;
    template<class A> void fields(A& a){a(method,place,partner,salience,interaction,object);}
};
class Planner {
public:
    static Decision choose(const PersonalView& view,const Seed& random);
    static std::vector<PlanOption> ideas(const PersonalView& view,const Seed& random);
    static Decision forecast(const PersonalView& view,const PlanOption& option);
};
struct Mind {
    SocialMemory social;
    Cognitive cognition;
    Attention attention;
    Learning learning;
    Knowledge knowledge;
    std::vector<Relation> relations;
    std::vector<KnownPlace> places;
    std::vector<KnownEdge> map;
    std::array<bool,method_count> known{};
    std::array<Outcomes,method_count> priors{};
    std::array<double,method_count> norms{},risks{};
    std::uint64_t decisions=0,version=1;
    int believed_food=0;double believed_money=0;
    Relation* relation(Id id);
    const Relation* relation(Id id)const;
    PersonalView view(Id self,Id place,Id home,Tick now,const Capability& c,const Outcomes& need,const std::vector<Id>& visible)const;
    template<class A> void fields(A& a){a(cognition,attention,learning,knowledge,relations,places,map,known,priors,norms,risks,decisions,version,believed_food,believed_money,social);}
};
} // namespace life
