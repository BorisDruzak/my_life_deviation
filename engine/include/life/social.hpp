#pragma once
#include "life/semantic_types.hpp"
#include "life/community.hpp"
#include <array>
#include <string>
#include <vector>

namespace life {
// All actors in this research profile are adults. A gender label is only a
// known/self-described attribute; it does NOT encode preference or morality.
enum class Gender:std::uint8_t {Unknown,Woman,Man};
enum class Interaction:std::uint8_t {
    FriendlyTouch,RomanticTouch,Compliment,AskInfo,ShowItem,BorrowItem,ReturnItem,ClaimItem,UseItem,ShareNews,DiscussTopic,Introduce,InviteMeeting,PartnerIntimacy,RequestWork,PraiseWork,Count
};
constexpr std::size_t interaction_count=std::size_t(Interaction::Count);
enum class SocialNorm:std::uint8_t {Boundary,Privacy,Property,Modesty,PersonalRomance,Honesty,Promise,Count};
constexpr std::size_t social_norm_count=std::size_t(SocialNorm::Count);
enum class ObjectKind:std::uint8_t {Book,Tool,Keepsake};
enum class SocialStage:std::uint8_t {Offer,Accepted,Declined,Completed,Cancelled,Used};
enum class SocialReason:std::uint8_t {None,NotInterested,Busy,NotUnderstood,Boundary,Unavailable,Withdrawn};
enum class SocialPhase:std::uint8_t {Proposed,Accepted,Completed,Declined,Cancelled};
enum class FactOrigin:std::uint8_t {InitialBelief,Observed,Reported,Agreement};
const char* interaction_name(Interaction kind);
const char* interaction_ru(Interaction kind);
const char* social_reason_name(SocialReason reason);
Tick interaction_duration(Interaction kind);
bool needs_joint_consent(Interaction kind);

struct MeetingProposal {
    Id place=0,host=0,route_from=0;std::uint32_t route_seconds=0;
    Tick at=0,until=0;bool private_visit=false;
    template<class A>void fields(A& a){a(place,host,route_from,route_seconds,at,until,private_visit);}
};
struct MethodBelief {
    double mastery=0,expected_pleasure=0,expected_acceptance=.6,confidence=.7;
    std::uint64_t source=0;
    template<class A>void fields(A& a){a(mastery,expected_pleasure,expected_acceptance,confidence,source);}
};
struct ItemClaim {
    Id speaker=0,owner=0,holder=0;Truth owner_status=Truth::Unknown,holder_status=Truth::Unknown;
    std::uint64_t source=0;FactOrigin origin=FactOrigin::Reported;Tick at=0;
    template<class A>void fields(A& a){a(speaker,owner,holder,owner_status,holder_status,source,origin,at);}
};
struct ItemMemory {
    Id id=0,owner=0,holder=0,place=0;ObjectKind kind=ObjectKind::Book;
    Truth owner_status=Truth::Unknown,holder_status=Truth::Unknown;
    double expected_use=.5,prestige=.3;std::uint32_t uses=0;
    FactOrigin origin=FactOrigin::InitialBelief;std::uint64_t source=0;Tick at=0;
    std::vector<ItemClaim> claims;
    template<class A>void fields(A& a){a(id,owner,holder,place,kind,owner_status,holder_status,expected_use,prestige,uses,origin,source,at,claims);}
};
struct PersonBelief {
    Id id=0;Gender gender=Gender::Unknown;double competence=.5;
    std::uint64_t source=0;Appearance appearance;bool appearance_known=false,known_kin=false;
    template<class A>void fields(A& a){a(id,gender,competence,source,appearance,appearance_known,known_kin);}
};
struct ContextExperience {
    Interaction kind=Interaction::FriendlyTouch;Id person=0,object=0;bool public_context=false;
    Estimate pleasure,approval;
    double accepted=1,refused=1;
    std::uint32_t attempts=0,boundaries=0,incoming=0,issued_boundaries=0;Tick last=0;
    std::uint64_t last_event=0;
    template<class A>void fields(A& a){a(kind,person,object,public_context,pleasure,approval,accepted,refused,attempts,boundaries,incoming,issued_boundaries,last,last_event);}
};
struct InteractionObservation {
    std::uint64_t delivery=0,event=0,parent=0;Id other=0,object=0;
    Interaction kind=Interaction::FriendlyTouch;SocialStage stage=SocialStage::Offer;
    SocialReason reason=SocialReason::None;bool public_context=false,initiated=false;
    double pleasure=0,primary_pleasure=0,approval=0,dose=1;bool pleasure_observed=false,approval_observed=false;
    Tick at=0;ItemMemory item;bool item_present=false;
    MethodBelief lesson;Interaction lesson_kind=Interaction::FriendlyTouch;bool lesson_present=false;
    Information information;bool information_present=false;
    MeetingProposal meeting;std::uint64_t project=0,outcome_source=0;
    template<class A>void fields(A& a){a(delivery,event,parent,other,object,kind,stage,reason,public_context,initiated,pleasure,primary_pleasure,approval,dose,pleasure_observed,approval_observed,at,item,item_present,lesson,lesson_kind,lesson_present,information,information_present,meeting,project,outcome_source);}
};
struct SocialMemory {
    CommunityMemory community;
    bool enabled=true;
    Gender gender=Gender::Unknown;
    std::array<MethodBelief,interaction_count> methods{};
    std::array<double,social_norm_count> norms{.7,.25,.8,.2,0,.7,.7};
    std::array<double,3> expected_attraction{0,.6,.6};
    // Cultural expectations are personal beliefs, not the current world's verdict.
    double public_romance_disapproval=.35,public_boasting_disapproval=.3,status_importance=.35;
    double rejection_sensitivity=.35;
    Id goal_object=0;bool goal_used=false;
    std::vector<ItemMemory> items;
    std::vector<PersonBelief> people;
    std::vector<ContextExperience> experiences;
    std::vector<InteractionObservation> inbox;
    std::vector<std::uint64_t> processed;
    std::uint64_t active_event=0,revision=1;
    std::uint64_t learned_outcomes=0,learned_reports=0,refusals_received=0,pressure_received=0;
    template<class A>void fields(A& a){a(community,enabled,gender,methods,norms,expected_attraction,public_romance_disapproval,public_boasting_disapproval,status_importance,rejection_sensitivity,goal_object,goal_used,items,people,experiences,inbox,processed,active_event,revision,learned_outcomes,learned_reports,refusals_received,pressure_received);}
    const ItemMemory* item(Id id)const;
    ItemMemory* item(Id id);
    const PersonBelief* person(Id id)const;
    const ContextExperience* experience(Interaction kind,Id other,Id object,bool public_context)const;
    bool observe(const InteractionObservation& obs,const Cognitive& capability,double current_memory);
    void report_item(const ItemMemory& reported,Id speaker,std::uint64_t source,Tick at,bool direct,FactOrigin direct_origin=FactOrigin::Agreement);
};
struct SocialTraits {
    std::array<double,interaction_count> pleasure{.7,.6,.4,.05,.15,.1,.1,.1,.35};
    std::array<double,3> attraction{0,.6,.6};
    // What the person truly enjoys isn't automatically known to their planner.
    double admiration=.6;Preference preference;
    template<class A>void fields(A& a){a(pleasure,attraction,admiration,preference);}
};
struct SocialView {
    bool enabled=false,in_conversation=false,occupied=false;Id self=0,partner=0,place=0;
    Tick now=0;std::uint64_t parent=0;
    std::uint32_t known_audience=0;
    // This is copied OWN memory, never the other participant's private state.
    SocialMemory memory;
    std::vector<PersonBelief> perceived;
    // Derived from the owner's Relation records, never persisted twice in SocialMemory.
    std::vector<std::pair<Id,double>> familiarities;
    double familiarity(Id person) const;
    double need_social=0,need_leisure=0;
    bool life_enabled=false;double need_desire=0;
    Id supervisor=0;bool employed=false;
    MeetingProposal proposal;
    template<class A>void fields(A& a){a(enabled,in_conversation,occupied,self,partner,place,now,parent,known_audience,memory,perceived,familiarities,need_social,need_leisure,life_enabled,need_desire,supervisor,employed,proposal);}
};
struct SocialEvaluation {
    bool known=false;double score=0,pleasure=0,acceptance=.5,moral=0,devaluation=0,status_gain=0;
    double instrumental=0,repetition=0;std::uint64_t basis=0;
    template<class A>void fields(A& a){a(known,score,pleasure,acceptance,moral,devaluation,status_gain,instrumental,repetition,basis);}
};
SocialEvaluation evaluate_social(const SocialView& view,Interaction kind,Id partner,Id object,bool receiving=false);
struct SocialChoice {Interaction kind;Id other=0,object=0;double priority=0;};
std::vector<SocialChoice> social_choices(const SocialView& view);

struct WorldObject {
    Id id=0,owner=0,holder=0,place=0;ObjectKind kind=ObjectKind::Book;
    double condition=1,prestige=.3;std::uint64_t active_loan=0;
    bool readable=true;Interaction lesson_kind=Interaction::FriendlyTouch;
    MethodBelief lesson;std::uint32_t uses=0;
    template<class A>void fields(A& a){a(id,owner,holder,place,kind,condition,prestige,active_loan,readable,lesson_kind,lesson,uses);}
};
struct Loan {
    std::uint64_t id=0;Id object=0,lender=0,borrower=0;Tick due=0;bool returned=false;
    template<class A>void fields(A& a){a(id,object,lender,borrower,due,returned);}
};
struct SocialEvent {
    std::uint64_t id=0,parent=0;Id initiator=0,receiver=0,object=0;
    Interaction kind=Interaction::FriendlyTouch;SocialPhase phase=SocialPhase::Proposed;
    SocialReason reason=SocialReason::None;
    Tick proposed_at=0,answered_at=0,ends=0;
    bool public_a=false,public_b=false;
    double primary_a=0,primary_b=0,pleasure_a=0,pleasure_b=0,approval_a=0,approval_b=0;
    SocialEvaluation response;Information information;bool information_present=false;std::vector<Id> listeners;std::vector<std::uint64_t> listener_parents;
    MeetingProposal meeting;std::uint64_t project=0,outcome_source=0;
    template<class A>void fields(A& a){a(id,parent,initiator,receiver,object,kind,phase,reason,proposed_at,answered_at,ends,public_a,public_b,primary_a,primary_b,pleasure_a,pleasure_b,approval_a,approval_b,response,information,information_present,listeners,listener_parents,meeting,project,outcome_source);}
};
struct SocialWorldState {
    std::vector<WorldObject> objects;
    std::vector<Loan> loans;
    std::vector<SocialEvent> events; // append-only audit for this lab; active lookup uses Actor active_event
    std::array<std::uint64_t,interaction_count> offered{},accepted{},completed{},declined{};
    std::uint64_t cancelled=0,uses=0,reports=0;
    template<class A>void fields(A& a){a(objects,loans,events,offered,accepted,completed,declined,cancelled,uses,reports);}
};
void validate_social_memory(const SocialMemory& memory);
} // namespace life
