#pragma once
#include "life/semantic_types.hpp"
#include "life/career.hpp"
#include <array>
#include <optional>
#include <vector>
#include <string>

namespace life {
enum class NewsKind:std::uint8_t {Visit,Employment,Purchase,Help,Taking,Skill,Opinion,Count};
enum class Disclosure:std::uint8_t {Open,Personal,Entrusted};
enum class NewsOrigin:std::uint8_t {Initial,Observed,Reported};
struct Information {
    bool job_terms_present=false;JobKnowledge job_terms;
    std::uint64_t id=0; NewsKind kind=NewsKind::Visit; Id subject=0,other=0,location=0;
    double confidence=.8,valence=0,material=0,importance=.3,sensitivity=0;
    Disclosure disclosure=Disclosure::Open; NewsOrigin origin=NewsOrigin::Observed;
    // cited_source is part of the communicated assertion, not a hidden world root.
    Id cited_source=0; Tick occurred_at=0;
    template<class A>void fields(A& a){a(job_terms_present,job_terms,id,kind,subject,other,location,confidence,valence,material,importance,sensitivity,disclosure,origin,cited_source,occurred_at);}
};
struct MemoryDetail {
    double strength=1,half_life_hours=24; Tick anchored=0;
    double at(Tick now)const;
    void reinforce(Tick now,double amount);
    template<class A>void fields(A& a){a(strength,half_life_hours,anchored);}
};
struct NewsMemory {
    Information content; Id speaker=0;
    MemoryDetail gist,place,time,source;
    std::uint64_t first_delivery=0;Tick encoded_at=0;
    bool pinned=false;
    template<class A>void fields(A& a){a(content,speaker,gist,place,time,source,first_delivery,encoded_at,pinned);}
};
struct SharedRecord {
    std::uint64_t claim=0;Id listener=0;Tick at=0;
    template<class A>void fields(A& a){a(claim,listener,at);}
};
struct Appearance {
    double height_cm=170,build=.5;std::uint8_t hair=0;
    template<class A>void fields(A& a){a(height_cm,build,hair);}
};
struct Preference {
    double preferred_height=170,preferred_build=.5,height_weight=.3,build_weight=.2;
    std::array<double,4> hair_liking{.5,.5,.5,.5};
    template<class A>void fields(A& a){a(preferred_height,preferred_build,height_weight,build_weight,hair_liking);}
};
struct ReputationView {
    std::optional<double> material,helpfulness,honesty;
    double coverage=0;std::size_t sources=0;
};
struct CommunityMemory {
    bool enabled=false;Appearance appearance;Preference preference;
    double confidentiality=.7,curiosity=.5,ambition=.5;
    double stranger_romance=.7,kin_romance=.9;
    std::vector<NewsMemory> entries;
    std::vector<std::uint64_t> delivered; // transport idempotency, not recollection
    std::vector<SharedRecord> shared;
    std::uint64_t received=0,forgotten=0,retrievals=0,revision=1;
    const NewsMemory* find(std::uint64_t id)const;
    bool receive(const Information& fact,Id speaker,std::uint64_t delivery,Tick received_at);
    std::optional<Information> recall(std::uint64_t id,Tick now)const;
    void rehearse(std::uint64_t id,Tick now);
    void forget(Tick now);
    std::vector<NewsMemory> candidates(Tick now,std::size_t limit=16)const;
    double disclosure_cost(const Information& info,double trust=.5)const;
    double topic_score(const NewsMemory& memory,Id listener,Tick now)const;
    ReputationView opinion(Id person,Tick now)const;
    template<class A>void fields(A& a){a(enabled,appearance,preference,confidentiality,curiosity,ambition,stranger_romance,kin_romance,entries,delivered,shared,received,forgotten,retrievals,revision);}
};
double attraction(const Appearance& seen,const Preference& preference);
struct JobOffer {const char* name;double hourly;unsigned skill;};
const std::array<JobOffer,4>& job_catalogue();
struct Recreation {const char* name;double fee,pleasure,load;};
const std::array<Recreation,6>& recreation_catalogue();
enum class TimeUse:std::uint8_t {Idle,Travel,Eating,Drinking,Sleep,Rest,Leisure,Social,Private,Work,Study,Other,Count};
constexpr std::size_t time_use_count=std::size_t(TimeUse::Count);
struct Economy {
    unsigned job=0;std::int64_t day_index=0;double work_today=0,study_today=0;
    double earned=0,food_spent=0,leisure_spent=0,study_spent=0,rent_paid=0,rent_debt=0;
    std::uint64_t promotions=0,study_sessions=0;double wage_known=12.5;
    std::array<double,6> interests{.8,.65,.7,.7,.6,.6};
    std::array<double,time_use_count> time_ms{};
    template<class A>void fields(A& a){a(job,day_index,work_today,study_today,earned,food_spent,leisure_spent,study_spent,rent_paid,rent_debt,promotions,study_sessions,wage_known,interests,time_ms);}
};
struct EconomyView {
    bool enabled=false;double reserve=100,protected_cash=0,study_budget=0,work_today=0,study_today=0;
    unsigned job=0;std::array<double,4> mastery{};std::array<double,6> interests{};
    template<class A>void fields(A& a){a(enabled,reserve,protected_cash,study_budget,work_today,study_today,job,mastery,interests);}
};
struct CommunityState {
    bool enabled=false;Tick next_day=86400000,next_forget=3600000;
    std::uint64_t utterances=0,deliveries=0,group_deliveries=0,missed=0,disclosures=0;
    std::uint64_t observed_events=0,observed_projections=0,paid_leisure=0;
    double speech_seconds=0,third_party_seconds=0;
    std::array<std::uint64_t,std::size_t(NewsKind::Count)> topics{};
    template<class A>void fields(A& a){a(enabled,next_day,next_forget,utterances,deliveries,group_deliveries,missed,disclosures,observed_events,observed_projections,paid_leisure,speech_seconds,third_party_seconds,topics);}
};
} // namespace life
