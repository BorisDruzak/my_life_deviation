#pragma once
#include "life/community.hpp"
#include "life/projects.hpp"
#include "life/activity_resources.hpp"
#include <optional>
namespace life {
struct RetailOffer {Id sku=1;double price=35;std::uint8_t tier=0;template<class A>void fields(A& a){a(sku,price,tier);}};
struct ShopKnowledge {
    Id place=0;std::uint64_t source=0;Tick observed_at=0,retry_at=0;
    std::vector<RetailOffer> offers;
    template<class A>void fields(A& a){a(place,source,observed_at,retry_at,offers);}
};
struct ProcedureLesson {
    std::uint64_t root=0;Id teacher=0;KnownProcedure procedure;
    template<class A>void fields(A& a){a(root,teacher,procedure);}
};
struct StoredLesson {
    ProcedureLesson content;std::uint16_t missing_steps=0;Tick at=0;double understanding=0;
    template<class A>void fields(A& a){a(content,missing_steps,at,understanding);}
};
struct LessonMemory {
    std::vector<StoredLesson> lessons;std::uint64_t retired_root=0,received=0;
    template<class A>void fields(A& a){a(lessons,retired_root,received);}
};
bool learn_procedure(ProjectMemory&,LessonMemory&,const ProcedureLesson&,std::uint16_t known_steps,double understanding,Tick now);
enum class LetterKind:std::uint8_t {Greeting,Information,Procedure,CancelMeeting};
struct LetterContent {
    LetterKind kind=LetterKind::Greeting;
    Information information;ProcedureLesson lesson;
    std::uint64_t appointment=0;bool reply=false;
    template<class A>void fields(A& a){a(kind,information,lesson,appointment,reply);}
};
struct PhoneAddress {Id person=0;std::uint64_t number=0,source=0;Tick at=0;template<class A>void fields(A& a){a(person,number,source,at);}};
struct Draft {
    Id id=0,person=0;LetterContent content;std::uint64_t basis=0;Tick created=0,retry_at=0;
    template<class A>void fields(A& a){a(id,person,content,basis,created,retry_at);}
};
struct CivilTrace {
    Tick at=0;std::uint64_t source=0,action=0;Id person=0,object=0;std::string kind;
    template<class A>void fields(A& a){a(at,source,action,person,object,kind);}
};
struct CivilMemory {
    bool enabled=false;Id own_garment=0,own_phone=0;double garment_condition=1;std::uint8_t garment_tier=0,desired_tier=0;
    double chosen_leisure_cost=0;Tick next_contact=0,next_observe=0;Id next_draft=1;
    std::vector<ShopKnowledge> shops;std::vector<PhoneAddress> contacts;
    std::vector<Draft> drafts;std::vector<std::uint64_t> inbox,read_messages;
    std::vector<std::uint64_t> cancelled_meetings;
    QuestionMemory questions;LessonMemory lessons;
    std::vector<CivilTrace> trace;
    std::uint64_t sent=0,read=0,clothes_bought=0,gifts_received=0,gifts_given=0,procedures_acquired=0;
    double clothing_spent=0,received_money=0,given_money=0;
    std::optional<std::uint64_t> number_for(Id person)const;
    bool learn_number(Id person,std::uint64_t number,std::uint64_t source,Tick now);
    Id compose(Id person,LetterContent content,std::uint64_t basis,Tick now);
    void record(CivilTrace t);
    template<class A>void fields(A& a){a(enabled,own_garment,own_phone,garment_condition,garment_tier,desired_tier,chosen_leisure_cost,next_contact,next_observe,next_draft,shops,contacts,drafts,inbox,read_messages,cancelled_meetings,questions,lessons,trace,sent,read,clothes_bought,gifts_received,gifts_given,procedures_acquired,clothing_spent,received_money,given_money);}
};
struct CivilBudget {double protected_cash=0,clothing_goal=0,leisure_goal=0,target=0,pressure=0;};
CivilBudget civil_budget(const CivilMemory&,double known_money,int known_food,Tick now=0);
double worn_condition(double old,double wear_per_day,double seconds);
struct CivilEquipment {
    Id garment=0,phone=0;std::uint64_t side_action=0,message=0;Id draft=0;
    Tick side_started=0,side_end=0;bool reading=false;
    template<class A>void fields(A& a){a(garment,phone,side_action,message,draft,side_started,side_end,reading);}
};
struct CivilPlanView {
    bool enabled=false,can_text=false;CivilMemory memory;CivilBudget budget;
    std::vector<Id> help_candidates;
    template<class A>void fields(A& a){a(enabled,can_text,memory,budget.protected_cash,budget.clothing_goal,budget.leisure_goal,budget.target,budget.pressure,help_candidates);}
};
struct RetailStock {Id object=0,place=0,sku=0;double price=0;bool sold=false;template<class A>void fields(A& a){a(object,place,sku,price,sold);}};
struct RetailShop {Id place=0,organization=0;std::vector<RetailOffer> catalogue;template<class A>void fields(A& a){a(place,organization,catalogue);}};
struct Envelope {
    std::uint64_t id=0,number=0,source_action=0,from_number=0;Id sender=0,receiver=0;
    LetterContent content;Tick sent_at=0,deliver_at=0;bool delivered=false,read=false,failed=false;
    template<class A>void fields(A& a){a(id,number,source_action,from_number,sender,receiver,content,sent_at,deliver_at,delivered,read,failed);}
};
struct CivilRuntime {
    bool enabled=false;Tick next_supply=86400000,next_observe=0;
    std::vector<RetailShop> shops;std::vector<RetailStock> stock;std::vector<Envelope> messages;
    std::uint64_t garments_initial=0,garments_supplied=0,purchases=0,sends=0,deliveries=0,reads=0,failed_messages=0,gifts=0,lessons=0;
    double money_transferred=0;std::vector<CivilTrace> trace;
    template<class A>void fields(A& a){a(enabled,next_supply,next_observe,shops,stock,messages,garments_initial,garments_supplied,purchases,sends,deliveries,reads,failed_messages,gifts,lessons,money_transferred,trace);}
};
struct PersonalView;struct PlanOption;struct Decision;
std::vector<PlanOption> civil_options(const PersonalView&);
double civil_expected_gain(const PersonalView&,const PlanOption&);
}
