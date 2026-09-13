#pragma once
#include "life/semantic_types.hpp"
#include <optional>
#include <vector>
namespace life {
// These are claims about different propositions, never a universal status score.
enum class ResourceKind:std::uint8_t {IncomeHourly,VisibleClothing,LeisureSpending,HelpReceived};
struct ResourceFact {
    std::uint64_t root=0;Id subject=0,object=0,speaker=0;
    ResourceKind kind=ResourceKind::IncomeHourly;
    double value=0,confidence=.8;Tick at=0;bool reported=false;
    void validate()const;
    template<class A>void fields(A& a){a(root,subject,object,speaker,kind,value,confidence,at,reported);}
};
struct ResourceAssessment {
    std::optional<double> hourly,clothing_tier,leisure_spend;
    bool may_have_spare_money=false;
    std::vector<std::uint64_t> bases;
};
struct ResourceKnowledge {
    bool enabled=false;std::vector<ResourceFact> facts;
    std::uint64_t revision=1,retired_root=0;
    bool receive(ResourceFact fact,Id speaker,Tick now);
    ResourceAssessment assess(Id subject,Tick now)const;
    void validate(Tick now)const;
    template<class A>void fields(A& a){a(enabled,facts,revision,retired_root);}
};
enum class QuestionKind:std::uint8_t {Clothing,Money,Contact,Procedure};
struct OpenQuestion {
    QuestionKind kind=QuestionKind::Clothing;Id target=0;
    std::uint64_t basis=0,reviewed_basis=0,revision=1;
    Tick changed=0,retry_at=0;bool pending=true,actionable=false;
    template<class A>void fields(A& a){a(kind,target,basis,reviewed_basis,revision,changed,retry_at,pending,actionable);}
};
struct QuestionMemory {
    std::vector<OpenQuestion> items;std::uint64_t reviews=0,reopened=0,dropped=0;
    OpenQuestion* find(QuestionKind kind,Id target);
    const OpenQuestion* find(QuestionKind kind,Id target)const;
    OpenQuestion& ensure(QuestionKind kind,Id target,std::uint64_t basis,Tick now);
    bool wake(QuestionKind kind,Id target,std::uint64_t basis,Tick now);
    void validate(Tick now)const;
    template<class A>void fields(A& a){a(items,reviews,reopened,dropped);}
};
}
