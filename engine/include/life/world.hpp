#pragma once
#include "life/mind.hpp"
#include "life/cognition.hpp"
#include <functional>
#include <map>
#include <string>
#include <stdexcept>

namespace life {
struct PersonRecord {
    Id id=0;std::int32_t birth_day=0;std::array<Id,2> parents{};bool active=true;
    template<class A> void fields(A& a){a(id,birth_day,parents,active);}
};
struct RoleLink {
    Id a=0,b=0;std::uint8_t kind=0;Tick since=0;std::uint64_t origin=0;
    template<class A> void fields(A& ar){ar(a,b,kind,since,origin);}
};
struct HistoryEvent {
    std::uint64_t id=0;Id a=0,b=0,place=0;Tick start=0,end=0;std::uint8_t kind=0;
    template<class A> void fields(A& ar){ar(id,a,b,place,start,end,kind);}
};
struct Place {
    Id id=0;std::uint32_t services=0;
    std::int64_t food=0,water=0,reserved=0;
    double account=0,temperature=0;Id home_owner=0;
    int job_level=-1,recreation=-1;double fee=0,wage=0;
    template<class A> void fields(A& a){a(id,services,food,water,reserved,account,temperature,home_owner,job_level,recreation,fee,wage);}
};
struct Edge {
    Id a=0,b=0;std::uint32_t seconds=0;bool open=true;
    template<class A> void fields(A& ar){ar(a,b,seconds,open);}
};
enum class Phase:std::uint8_t {Idle,Travel,Running,SleepAttempt,Asleep,Waiting};
struct Action {
    DecisionExperience decision_experience;
    Method method=Method::Idle;Phase phase=Phase::Idle;
    std::uint64_t id=0;Id destination=0,partner=0;
    std::vector<Id> path;std::uint32_t leg=0;
    Tick started=0,end=0,window_start=0,next_window=0,segment_start=0;
    bool reserved=false,blocked=false;
    double chosen_score=0,pleasure_sum=0,primary_sum=0,desire_primary_sum=0;
    double samples=0;
    std::vector<std::pair<Id,double>> feature_sum;
    Outcomes before{};Id object=0;
    std::uint64_t project=0;StepKind project_step=StepKind::Visit;double effort_seconds=0;
    template<class A> void fields(A& a){a(decision_experience,method,phase,id,destination,partner,path,leg,started,end,window_start,next_window,segment_start,reserved,blocked,chosen_score,pleasure_sum,primary_sum,desire_primary_sum,samples,feature_sum,before,object,project,project_step,effort_seconds);}
};
struct Actor {
    Id id=0,home=0,place=0,style=0,household=0;
    Biology biology;Body body;Desire desire;Affect affect;Mind mind;
    CognitiveState cog;Tick physical_at=0;double cognition_rate_snapshot=0;
    double leisure=.7,social=.7,esteem=0;
    std::array<double,method_count> satiation{};
    std::array<double,8> innate_trigger{};
    std::vector<Feature> exposure;
    double primary_trigger=0;
    std::int64_t food=1;double money=10;
    Action action;Tick review=0;ParallelConversation conversation;EmploymentContract employment;
    double secondary_social_ms=0,partner_ms=0;std::vector<MeetingAgreement> appointments;
    SocialTraits social_traits;Economy economy;Appearance appearance;
    bool alive=true;
    std::array<std::uint64_t,method_count> completed{};
    std::uint64_t failures=0,violations=0,blocked_seconds=0,consumed_food=0,consumed_water=0;double critical_seconds=0;
    std::array<double,3> critical_by_need{},critical_current{},critical_longest{};
    std::array<std::uint64_t,3> critical_episodes{};
    std::vector<std::pair<std::string,std::uint64_t>> failure_reasons;
    double min_energy=.8,min_water=.8,max_damage=0;
    template<class A> void fields(A& a){a(critical_by_need,critical_current,critical_longest,critical_episodes,failure_reasons,id,home,place,style,household,biology,body,desire,affect,mind,cog,physical_at,cognition_rate_snapshot,leisure,social,esteem,satiation,innate_trigger,exposure,primary_trigger,food,money,action,review,social_traits,economy,appearance,alive,completed,failures,violations,critical_seconds,blocked_seconds,consumed_food,consumed_water,min_energy,min_water,max_damage,conversation,employment,secondary_social_ms,partner_ms,appointments);}
};
struct Ledger {
    std::int64_t initial_food=0,supplied_food=0,eaten_food=0,initial_water=0,supplied_water=0,drunk_water=0;
    double initial_money=0,wages=0;
    std::uint64_t supplies=0;
    template<class A> void fields(A& a){a(initial_food,supplied_food,eaten_food,initial_water,supplied_water,drunk_water,initial_money,wages,supplies);}
};
struct State {
    std::string format="LIFE-0.12.0-self1",balance="balance-cpp-0.9.0-social1",scenario="normal";
    SocialWorldState social;CommunityState community;LifeRuntime life;
    Seed random;Tick now=0,next_supply=86400000,next_physical=0;
    std::uint64_t next_id=1;
    bool autonomy=true,self_enabled=false,self_effects=true;
    bool adaptive_life=false;std::vector<HiringOffice> hiring;
    std::vector<PersonRecord> people;
    std::vector<HistoryEvent> history;
    std::vector<RoleLink> roles;
    std::vector<Place> places;
    std::vector<Edge> edges;
    std::vector<Actor> actors;
    Ledger ledger;
    template<class A> void fields(A& a){a(adaptive_life,hiring,self_enabled,self_effects,format,balance,scenario,random,now,next_supply,next_physical,next_id,autonomy,people,history,roles,places,edges,actors,ledger,social,community,life);}
};
struct EventLog {
    Tick time;Id actor;std::uint64_t event;Method method;
    std::string kind,result;Id place=0,partner=0;
    double score=0,moral=0,risk=0;
    int operations=0,slots=0;
    Id object=0;std::uint64_t parent=0;Interaction interaction=Interaction::FriendlyTouch;
};
struct Metrics {
    std::uint64_t body_steps=0,perception_candidates=0,decisions=0;
};
class World {
    State state_;
    bool indexed_=true;
    std::vector<std::vector<Id>> occupants_;
    std::map<std::pair<Id,Id>,std::vector<Id>> road_occupants_;
    std::vector<Id> local_people(const Actor& a);
    std::function<void(const EventLog&)> logger_;
    std::function<void(const Thought&)> thought_logger_;
    SelfTraceSink self_logger_;
    void publish_outcome(Actor& actor,OutcomeSignal signal);
    void publish_action_outcome(Actor& actor,bool completed,bool blocked,const Outcomes& observed={},std::uint16_t mask=0,bool interrupted=false);
    void capture_self_decision(Actor& actor,std::uint64_t action,const Decision& decision);
    void append_self_topics(const Actor& actor,std::vector<AttentionTopic>& input) const;
    bool start_self_cognition(Actor& actor);
    bool complete_self_cognition(Actor& actor,Operation operation,Tick started);
    void publish_social_outcome(Actor& actor,const InteractionObservation& observation);
    void validate_self_runtime() const;
    void populate_career_view(const Actor&,PersonalView&)const;
    void append_career_topics(const Actor&,std::vector<AttentionTopic>&)const;
    bool start_career_cognition(Actor&);
    bool complete_career_cognition(Actor&,Operation,Tick);
    bool complete_career_action(Actor&);
    void receive_career_information(Actor&,const JobKnowledge&,std::uint64_t);
    void validate_career()const;

    std::shared_ptr<ForecastWorkers> workers_;std::size_t work_chunk_=64;
    void populate_life_view(const Actor& actor,PersonalView& view) const;
    void life_recall(Actor& actor);
    void life_commit(Actor& actor,const Decision& decision);
    void life_action_result(Actor& actor,StepOutcome outcome);
    void life_social_result(Actor& actor,const InteractionObservation& observation);
    void life_social_completed(SocialEvent& event);
    bool life_start_conversation(Actor& actor,const Decision& decision);
    void life_stop_conversation(Actor& actor,bool completed);
    void life_conversation_tick();
    void employment_tick();
    void employment_credit(Actor& actor);
    void validate_life() const;
    void physical_until(Actor& actor,Tick now);
    void populate_social_view(const Actor& actor,PersonalView& view) const;
    void start_social(Actor& actor,const Decision& decision);
    void process_social();
    void social_answer(Actor& actor,const InteractionObservation& offer,bool accepted,const SocialEvaluation& evaluation);
    double social_observe(Actor& actor,const InteractionObservation& observation);
    void social_deliver(Actor& actor,InteractionObservation observation);
    void stop_social(Actor& actor);
    bool complete_object_use(Actor& actor);
    void validate_social_world() const;
    void seed_social();
    void community_tick();
    void community_finish(Actor& actor,Method method);
    void community_publish(Actor& actor,Information info);
    void community_begin_utterance(SocialEvent& event);
    void community_complete_utterance(const SocialEvent& event);
    void populate_economy(const Actor& actor,PersonalView& view)const;
    void community_account(Actor& actor,Tick elapsed);
    void social_known_person(Actor& actor,Id person,std::uint64_t source);
    const SocialEvent* social_event(std::uint64_t id)const;
    SocialEvent* social_event(std::uint64_t id);
    void emit_social(const SocialEvent& event,Id actor,const char* result);
    struct PendingAnswer {Id actor=0;InteractionObservation offer;bool accepted=false;SocialEvaluation evaluation;};
    std::vector<PendingAnswer> pending_social_answers_;

    void cognition_tick(Actor& actor);
    void cognition_start(Actor& actor,Operation operation);
    void cognition_complete(Actor& actor,const Decision* forecast=nullptr);
    void update_senses(Actor& actor);
    void emit_thought(Actor& actor,Thought thought);
    void reply_observed(Actor& actor,Id partner,ReplyMessage message,std::uint64_t event);
    void process_boundary();
    Tick next_boundary() const;
    Metrics metrics_;
    std::vector<std::pair<Id,Decision>> pending_intents_;
    struct EffectCache {std::uint64_t revision=~0ull;std::size_t size=0;std::vector<Feature> input;PreparedEffects plan;};
    std::vector<EffectCache> effect_cache_; // Derived, discardable, intentionally absent from saves/hashes.
    void rebuild_index();
    void project(Actor& actor);
    void start(Actor& actor,const Decision& decision);
    void begin_at_destination(Actor& actor);
    void complete(Actor& actor);
    void finish_window(Actor& actor);
    void fail(Actor& actor,const char* reason);
    void cancel(Actor& actor);
    void step();
    void emit(const Actor& a,const char* kind,const char* result,const Decision* d=nullptr);
public:
    void configure_adaptive_life(bool self_effects=true);
    void career_information_for_test(Id id,const JobKnowledge& k,std::uint64_t source){receive_career_information(state_.actors.at(id-1),k,source);}
    std::string career_report_json()const;
    void configure_self_model(bool effects=true);
    void set_self_logger(SelfTraceSink logger){self_logger_=std::move(logger);}
    void social_observation_for_test(Id id,InteractionObservation o){social_deliver(state_.actors.at(id-1),std::move(o));}
    void observe_outcome_for_test(Id actor,OutcomeSignal signal){publish_outcome(state_.actors.at(actor-1),std::move(signal));}
    std::string self_report_json()const;
    void configure_life_projects(bool forecast_satiation=true);
    std::string life_report_json()const;
    bool conversation_for_test(Id actor,Id partner);
    void invitation_for_test(Id actor,Id partner,const MeetingProposal& proposal);
    void configure_social_scene(const std::string& scene);
    void configure_community();
    std::string community_report_json()const;
    void propose_social_for_test(Id actor,Interaction kind,Id other,Id object=0);
    void withdraw_social_for_test(Id actor);
    void use_object_for_test(Id actor,Id object);
    static World generate(std::uint64_t seed,std::size_t population=128,const std::string& scenario="normal");
    explicit World(State state={},bool indexed=true):state_(std::move(state)),indexed_(indexed){}
    const State& state()const{return state_;}
    State& edit_for_test(){effect_cache_.clear();return state_;} // Scenario authoring only; never passed to Planner.
    void set_indexed(bool value){indexed_=value;rebuild_index();}
    void set_logger(std::function<void(const EventLog&)> logger){logger_=std::move(logger);}
    const Metrics& metrics()const{return metrics_;}
    void run_seconds(std::uint64_t seconds);
    void run_ms(std::uint64_t milliseconds);
    void set_thought_logger(std::function<void(const Thought&)> logger){thought_logger_=std::move(logger);}
    void set_workers(unsigned n,std::size_t chunk=64){if(!chunk||chunk>100000)throw std::invalid_argument("work chunk");workers_=std::make_shared<ForecastWorkers>(n);work_chunk_=chunk;}
    void validate()const;
    void command_for_test(Id actor,Method method,Id destination);
    std::vector<Id> visible_for_test(Id actor);
    PersonalView personal_view(Id actor);
    void save(const std::string& path)const;
    static World load(const std::string& path,bool indexed=true);
    std::string hash()const;
    std::string summary_json()const;
    std::string social_report_json()const;
};
bool in_conversation(const Actor& actor);
std::uint64_t conversation_id(const Actor& actor);
Id conversation_partner(const Actor& actor);
bool conversation_together(const Actor& a,const Actor& b,std::uint64_t parent);
}
