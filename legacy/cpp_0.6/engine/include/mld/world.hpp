#pragma once
#include "mld/internal.hpp"
#include "mld/hash.hpp"
#include <ostream>
#include <queue>
namespace mld {
    inline constexpr const char* model_version="0.6.0-lab1";
    inline constexpr const char* catalog_text="mld-lab1;star-village;14-actions;food=0.35;drink=0.25;sleep-latency=120;reference-ms=1000;relief=0.6/1800;social=1800;work=1800/2;intent-continuation-v1;waiting-from-talk-Q;expected-sanctions-only";
    const std::string& catalog_hash();
    enum class Act:std::uint8_t {Wait,Move,Inspect,Exchange,Take,Eat,Drink,Sleep,Rest,Play,Talk,Relief,Work,Give,Count};
    constexpr std::size_t action_count=std::size_t(Act::Count),motives=8;
    const char* action_name(Act);
    struct Config {
        std::uint64_t seed=42;std::uint32_t population=128;bool scarce=false;double theft_norm_override=-1;bool temptation=false;
        std::string profile=model_version;
        MLD_FIELDS(seed,population,scarce,theft_norm_override,temptation,profile)
    };
    struct Person {
        Id id=none,parent=none;std::int32_t birth_day=0;bool active=true;
        MLD_FIELDS(id,parent,birth_day,active)
    };
    struct History {
        std::uint64_t id=0;Id a=none,b=none,place=0;Time start=0,end=0;std::uint8_t kind=0;
        MLD_FIELDS(id,a,b,place,start,end,kind)
    };
    struct Relation {
        Id person=none;bool known_relative=false;double care=.1;std::array<Stats,4> reliability{};
        std::uint64_t source=0;Time seen=0,retry=0;
        MLD_FIELDS(person,known_relative,care,reliability,source,seen,retry)
    };
    struct KnownPlace {
        Id place=none;Truth food=Truth::Unknown;bool free_food=false;Time checked=0,retry=0;std::uint64_t source=0;
        MLD_FIELDS(place,food,free_food,checked,retry,source)
    };
    struct Knowledge {
        std::vector<Relation> people;std::vector<KnownPlace> places;std::array<bool,action_count> ways{};
        std::array<std::array<Stats,motives>,action_count> expectation{};
        double property_norm=.7,privacy_norm=.5,sanction=.096;
        std::uint64_t version=0,last_learn=0;
        MLD_FIELDS(people,places,ways,expectation,property_norm,privacy_norm,sanction,version,last_learn)
    };
    struct Ongoing {
        Act type=Act::Wait;Time start=0,end=0;Id target=none;std::uint64_t id=0;Time last_window=0;
        double moral=0,expected=0;bool social_accepted=false;
        std::array<double,8> initial_sensed{};bool asleep=false;Time sleep_progress=0;
        MLD_FIELDS(type,start,end,target,id,last_window,moral,expected,social_accepted,initial_sensed,asleep,sleep_progress)
    };
    struct Intent {
        Act procedure=Act::Wait;Id destination=none;std::uint8_t motive=0;bool active=false;
        MLD_FIELDS(procedure,destination,motive,active)
    };
    struct Npc {
        Id id=none,home=none,place=0;Body body;Biology biology;Desire desire;
        double leisure=.7,social=.7,habituation=0,innate_trigger=0,trigger=0,pleasant=0;
        Emotions emotions{},targets{};Time target_until=0,next_review=0;
        Knowledge memory;Reactions reactions;Ongoing action;Intent intent;
        std::int64_t money=8,food=1,unowned_food=0;std::uint64_t decisions=0;
        std::array<double,8> sensed{};std::uint8_t focus=0;Time focus_since=0;
        std::array<std::uint64_t,action_count> completed{};
        MLD_FIELDS(id,home,place,body,biology,desire,leisure,social,habituation,innate_trigger,trigger,pleasant,emotions,targets,target_until,next_review,memory,reactions,action,intent,money,food,unowned_food,decisions,sensed,focus,focus_since,completed)
    };
    struct Place {
        Id id=none;std::uint8_t kind=0;std::int64_t food=0,water=0,money=0;bool closed=false;
        MLD_FIELDS(id,kind,food,water,money,closed)
    };
    struct Counters {
        std::uint64_t events=0,choices=0,operations=0,failures=0,unauthorized=0,transfers=0,reliefs=0,contacts=0;
        std::uint64_t digest=1469598103934665603ULL;
        std::int64_t initial_food=0,supplied_food=0,eaten=0,initial_water=0,supplied_water=0,drunk=0,initial_money=0,wages=0;
        std::array<std::uint64_t,7> critical_seconds{};
        MLD_FIELDS(events,choices,operations,failures,unauthorized,transfers,reliefs,contacts,digest,initial_food,supplied_food,eaten,initial_water,supplied_water,drunk,initial_money,wages,critical_seconds)
    };
    struct State {
        Config config;Time now=0;std::uint64_t sequence=0;
        std::vector<Person> persons;std::vector<History> history;std::vector<Npc> npcs;std::vector<Place> places;
        Counters counters;
        MLD_FIELDS(config,now,sequence,persons,history,npcs,places,counters)
    };
    // A controller cannot access physical objects, hidden stock, another mind or W.
    struct View {
        Id actor=none,home=none,place=none;Time now=0;std::array<double,8> sensed{},cognition{};
        std::int64_t food=0,money=0;std::vector<Id> seen_people;
        const Knowledge* knowledge=nullptr;Act current=Act::Wait;bool in_contact=false;Intent intent;
    };
    struct Choice {
        Act type=Act::Wait;Id target=none;double score=-1,moral=0,forecast=0;
        int used_operations=0,context_used=0;std::uint8_t motive=0;
        Act procedure=Act::Wait;Id destination=none;
    };
    Choice choose(const View&);
    bool accept_contact(const View&);
    State generate(Config);
    void validate(const State&);
    std::string encode(State);
    State decode(const std::string&);
    std::string semantic_hash(const State&);
    class World {
        State state_;bool indexed_;std::ostream* log_=nullptr;
        std::vector<std::vector<Id>> occupants_;
        using Due=std::pair<Time,Id>;
        std::priority_queue<Due,std::vector<Due>,std::greater<Due>> due_;
        void rebuild();void schedule(Id);void event(std::string_view,Id,Id=none,std::int64_t=0);
        void physiology(Npc&);void finish(Id);void review(Id);void start(Id,const Choice&);void experience(Npc&,bool);
        void boundary();void supply();void update_occupancy(Id,Id,Id);
        public:
        explicit World(Config c={},bool indexed=true);
        explicit World(State s,bool indexed=true);
        const State& state()const{return state_;}
        View view(Id)const;
        void advance(std::uint32_t seconds);
        void log_to(std::ostream* s){log_=s;}
        std::string snapshot()const;
        static World restore(const std::string&,bool indexed=true);
        std::string summary()const;
    };
}
