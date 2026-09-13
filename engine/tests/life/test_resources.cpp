#include "test.hpp"
#include "life/resources.hpp"
using namespace life;
namespace {
ResourceFact fact(std::uint64_t root,ResourceKind kind,double value,Id subject=2) {
    ResourceFact f; f.root=root;f.subject=subject;f.kind=kind;f.value=value;f.confidence=.8;return f;
}
}
TEST("resources", no_global_status_or_hidden_balance){ResourceKnowledge k;auto r=k.assess(2,0);CHECK(!r.hourly);CHECK(!r.clothing_tier);CHECK(!r.leisure_spend);CHECK(r.bases.empty());CHECK(!r.may_have_spare_money);}
TEST("resources", unrelated_expenses_do_not_reduce_salary){ResourceKnowledge k;k.receive(fact(1,ResourceKind::IncomeHourly,40),1,0);for(int i=0;i<10;++i)k.receive(fact(2+i,ResourceKind::LeisureSpending,2),1,0);NEAR(*k.assess(2,0).hourly,40,1e-12);}
TEST("resources", duplicate_root_is_not_independent_evidence){ResourceKnowledge k;auto f=fact(1,ResourceKind::VisibleClothing,2);CHECK(k.receive(f,1,0));auto r=k.revision;CHECK(!k.receive(f,3,1));CHECK(k.revision==r);CHECK(k.assess(2,1).bases.size()==1);}
TEST("resources", clothing_is_not_proof_of_spare_cash){ResourceKnowledge k;k.receive(fact(1,ResourceKind::VisibleClothing,2),1,0);auto r=k.assess(2,0);CHECK(r.clothing_tier);CHECK(!r.hourly);CHECK(!r.may_have_spare_money);}
TEST("resources", explicit_subjective_spare_resources_hypothesis){ResourceKnowledge k;k.receive(fact(1,ResourceKind::IncomeHourly,40),1,0);k.receive(fact(2,ResourceKind::LeisureSpending,25),1,0);auto r=k.assess(2,0);CHECK(r.may_have_spare_money);CHECK(r.bases.size()==2);}
TEST("resources", facts_about_others_do_not_leak_between_people){ResourceKnowledge k;k.receive(fact(1,ResourceKind::IncomeHourly,40),1,0);CHECK(!k.assess(3,0).hourly);}
TEST("resources", resource_validation){ResourceKnowledge k;auto f=fact(1,ResourceKind::IncomeHourly,-2);THROWS(k.receive(f,1,0));f=fact(0,ResourceKind::IncomeHourly,2);THROWS(k.receive(f,1,0));f=fact(1,ResourceKind::VisibleClothing,9);THROWS(k.receive(f,1,0));}
TEST("resources", old_evidence_can_expire_without_global_truth){ResourceKnowledge k;k.receive(fact(1,ResourceKind::IncomeHourly,40),1,0);CHECK(!k.assess(2,180LL*86400000).hourly);}
TEST("resources", bounded_resource_memory){ResourceKnowledge k;for(int i=1;i<600;++i)k.receive(fact(i,ResourceKind::LeisureSpending,i%30),1,i);CHECK(k.facts.size()<=128);k.validate(1000);}
TEST("resources", matching_fact_reopens_only_linked_question){QuestionMemory m;auto& a=m.ensure(QuestionKind::Clothing,3,10,0);a.pending=false;auto& b=m.ensure(QuestionKind::Contact,4,11,0);b.pending=false;CHECK(m.wake(QuestionKind::Clothing,3,12,100));CHECK(m.find(QuestionKind::Clothing,3)->pending);CHECK(!m.find(QuestionKind::Contact,4)->pending);}
TEST("resources", same_fact_does_not_restart_question){QuestionMemory m;auto& q=m.ensure(QuestionKind::Money,2,10,0);q.pending=false;CHECK(!m.wake(QuestionKind::Money,2,10,100));CHECK(!m.find(QuestionKind::Money,2)->pending);}
