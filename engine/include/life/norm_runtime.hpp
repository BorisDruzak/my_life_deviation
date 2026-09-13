#pragma once
#include "life/norm_memory.hpp"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace life {
// Content identifiers of the authored NORM-0.1 profile, not population truths.
enum class NormPractice : std::uint32_t {
    ClothingCondition=1, ClothingTier, Property, Honesty, Privacy, Promise,
    PersonalRomance, Boundary, Modesty, Work, Help, StrangerRomance, KinRomance
};
constexpr std::uint32_t practice_id(NormPractice p){return std::uint32_t(p);}
struct NormPayload {
    bool present=false,question=false,explanation=false;
    NormKey key{};NormObservation evidence{};
    Id subject=0;std::uint64_t observed_action=0;
    template<class A>void fields(A& a){a(present,question,explanation,key,evidence,subject,observed_action);}
};
struct KnownNormMember {
    Id person=0;std::uint64_t source=0;Tick at=0;
    template<class A>void fields(A& a){a(person,source,at);}
};
struct KnownNormGroup {
    Id id=0;std::uint32_t context=0;Id place=0;
    std::uint64_t source=0;std::vector<KnownNormMember> members;
    template<class A>void fields(A& a){a(id,context,place,source,members);}
};
struct NormGoalLink {
    std::uint64_t goal=0,source=0;Id group=0;
    double importance=0,relevance=0;bool active=false;
    template<class A>void fields(A& a){a(goal,source,group,importance,relevance,active);}
};
struct NormQuestion {
    std::uint64_t id=0,goal=0,basis=0,revision=0,reviewed_revision=0;
    NormKey key{};Tick changed=0,retry_at=0;
    double tension=0;bool active=false,pending=false,waiting=false;
    template<class A>void fields(A& a){a(id,goal,basis,revision,reviewed_revision,key,changed,retry_at,tension,active,pending,waiting);}
};
struct NormContextMemory {
    std::vector<KnownNormGroup> groups;
    std::vector<NormGoalLink> goals;
    std::vector<NormQuestion> questions;
    std::uint64_t next_question=1,revision=1,questions_woken=0;
    std::uint64_t employment_source=0;Id employment_group=1;
    double group_significance(Id group)const;
    const KnownNormGroup* group(Id id)const;
    bool member_known(Id group,Id person)const;
    void validate(Tick now)const;
    template<class A>void fields(A& a){a(groups,goals,questions,next_question,revision,questions_woken,employment_source,employment_group);}
};
// Aligned with the four paid predictions. No source archive or global audience.
struct NormEffectContext {
    std::uint64_t goal=0;Id audience=0;
    double group_significance=0,approval_value=0,disapproval_value=0;
    double applicability=1,exception=0;
    bool audience_known=false;
    template<class A>void fields(A& a){a(goal,audience,group_significance,approval_value,disapproval_value,applicability,exception,audience_known);}
};
struct NormPlanContext {
    std::array<NormEffectContext,4> effects{};
    bool question_active=false;NormKey question_key{};std::uint64_t question_id=0;
    template<class A>void fields(A& a){a(effects,question_active,question_key,question_id);}
};
struct NormCognitiveState {
    std::vector<NormObservation> inbox;
    std::vector<std::uint64_t> delivered;
    std::vector<NormExposureDose> sensed;
    NormObservation input{},interpreted{};
    NormPayload request;Id request_from=0;std::uint64_t request_delivery=0;Tick request_at=0;
    std::vector<NormPayload> outbound;
    NormContextSnapshot context{};
    NormDecisionView considered{};NormPlanContext plan_context{};
    Id prepared_place=0;Tick prepared_at=0;
    std::uint64_t prepared_revision=0,prepared_context=0;
    std::uint8_t captured_focus_metric=0;
    std::uint64_t captured_focus=0;
    std::uint8_t resume_operation=0;
    std::uint64_t reaction_considered=0;
    std::uint64_t captured_revision=0,captured_context=0,question=0;
    std::uint64_t published=0,dropped=0,integrated=0,replaced=0,duplicates=0;
    std::uint64_t shadow_integrated=0,interpreted_count=0,deep_evaluations=0;
    std::uint64_t candidates_seen=0,candidates_displaced=0;
    NormMemory frozen_shadow;
    Tick next_observe=0;
    template<class A>void fields(A& a){a(captured_focus_metric,captured_focus,prepared_place,prepared_at,prepared_revision,prepared_context,reaction_considered,request,request_from,request_delivery,request_at,outbound,resume_operation,inbox,delivered,sensed,input,interpreted,context,considered,plan_context,captured_revision,captured_context,question,published,dropped,integrated,replaced,duplicates,shadow_integrated,interpreted_count,deep_evaluations,candidates_seen,candidates_displaced,frozen_shadow,next_observe);}
};
struct NormTrace {
    Tick at=0;Id actor=0;std::string kind,reason;
    NormKey key{};std::uint64_t delivery=0,known_source=0,evidence_revision=0,own_revision=0;
    std::uint64_t question=0,action=0;double before=0,after=0,value=0;
    // A forecast producer revision is not an observed source root or event revision.
    std::uint64_t source_revision=0;
    std::uint8_t ledger_owner=255,consequence_kind=255,knownness=255;
    Id consequence_target=0,consequence_object=0;
    std::uint64_t consequence_horizon=0;
    double probability=1,time_hours=0;bool present_value=false;
    std::uint32_t candidate_method=0,candidate_interaction=0;Id candidate_object=0;
};
using NormTraceSink=std::function<void(const NormTrace&)>;
std::string norm_trace_json(const NormTrace& trace);
bool norm_learning_active(NormMode mode);
bool norm_effects_active(NormMode mode);
const char* norm_mode_name(NormMode mode);
NormMode parse_norm_mode(const std::string& value);
} // namespace life
