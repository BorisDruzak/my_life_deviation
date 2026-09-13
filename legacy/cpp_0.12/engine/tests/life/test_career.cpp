#include "test.hpp"
#if __has_include("life/career.hpp")
#include "life/career.hpp"
using namespace life;
namespace {JobKnowledge job(Id id=1){JobKnowledge k;k.organization=id;k.place=5;k.hourly=20;k.shift_start=32400000;k.shift_end=61200000;k.vacancy=true;k.required_skill=701;k.known=31;return k;}}
TEST("career",partial_salary_preserves_other_job_terms){
 CareerMemory m;auto k=job();CHECK(m.receive(k,1,0));auto partial=k;partial.known=WageKnown;partial.hourly=25;
 CHECK(m.receive(partial,2,1000));const auto* x=m.find(1);CHECK(x&&x->known==31);CHECK(x->vacancy);CHECK(x->required_skill==701);CHECK(x->shift_end==61200000);NEAR(x->hourly,25,0);
}
TEST("career",duplicate_same_salary_is_not_new_decision_basis){
 CareerMemory m;auto k=job();m.receive(k,1,0);auto revision=m.revision;
 CHECK(!m.receive(k,2,1000));CHECK(m.revision==revision);
}
TEST("career",higher_wage_without_vacancy_means_inquiry_not_hired){
 auto k=job();k.known=LocationKnown|WageKnown;
 CHECK(assess_career(k,12.5,true,.8).stage==CareerStage::Inquire);
}
TEST("career",lower_net_value_retains_current_work){
 auto k=job();k.hourly=10;CHECK(assess_career(k,20,true,.8).stage==CareerStage::Deferred);
}
TEST("career",complete_known_offer_can_be_applied_to){
 CHECK(assess_career(job(),12.5,true,.8).stage==CareerStage::Apply);
}
TEST("career",new_information_reopens_only_related_question){
 CareerMemory m;m.receive(job(1),1,0);m.receive(job(2),2,0);m.questions[0].stage=CareerStage::Deferred;m.questions[1].stage=CareerStage::Deferred;
 auto k=job(1);k.hourly=40;m.receive(k,3,1000);CHECK(m.questions[0].stage==CareerStage::Review);CHECK(m.questions[1].stage==CareerStage::Deferred);
}
#else
TEST("career",information_grounded_career_pipeline_must_exist){CHECK(false && "career information/applications absent in 0.11");}
#endif
#ifdef LIFE_CAREER_RUNTIME
TEST("career",missing_qualification_has_real_deferred_benefit_cost){
 auto unavailable=job(4);unavailable.hourly=21;unavailable.required_skill=702;
 auto accessible=job(3);accessible.hourly=18;accessible.required_skill=0;
 auto distant=assess_career(unavailable,12.5,true,0);
 auto ready=assess_career(accessible,12.5,true,1);
 CHECK(distant.stage==CareerStage::Study);CHECK(ready.stage==CareerStage::Apply);
 CHECK(distant.expected_gain<ready.expected_gain);
 auto almost=assess_career(unavailable,12.5,true,.59);
 CHECK(almost.expected_gain>distant.expected_gain);
}
#endif
