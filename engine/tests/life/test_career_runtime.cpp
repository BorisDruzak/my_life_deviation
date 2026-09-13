#include "test.hpp"
#include "life/world.hpp"
#include <filesystem>
using namespace life;
#ifdef LIFE_CAREER_RUNTIME
namespace {
World careers(){auto w=World::generate(42,8);w.configure_adaptive_life();return w;}
JobKnowledge complete_offer(Id org=2){JobKnowledge k;k.organization=org;k.place=7;k.hourly=20;k.shift_start=32400000;k.shift_end=61200000;k.vacancy=true;k.required_skill=0;k.known=31;return k;}
}
TEST("career_runtime",low_cash_and_sufficient_actual_job_capacity){
 auto w=careers();unsigned capacity=0;for(const auto& o:w.state().hiring)capacity+=o.capacity;CHECK(capacity>=8);
 unsigned unemployed=0;for(const auto& a:w.state().actors){CHECK(a.money>=18&&a.money<=36);unemployed+=!a.employment.active;}CHECK(unemployed>0);w.validate();
}
TEST("career_runtime",observed_fact_not_hidden_vacancy_reopens_question){
 auto w=careers();auto& a=w.edit_for_test().actors[3];a.mind.career.receive(complete_offer(),900000,0);
 for(auto& q:a.mind.career.questions)q.stage=CareerStage::Deferred;
 auto rev=a.mind.career.revision;w.edit_for_test().hiring[1].hourly=40;
 CHECK(a.mind.career.revision==rev);auto k=complete_offer();k.hourly=40;w.career_information_for_test(a.id,k,900001);
 CHECK(a.mind.career.find(2)->hourly==40);CHECK(a.mind.career.questions[1].stage==CareerStage::Review);
}
TEST("career_runtime",unemployed_applies_and_receives_real_employment){
 auto w=careers();w.edit_for_test().autonomy=false;w.run_seconds(9*3600);
 auto& a=w.edit_for_test().actors[3];CHECK(!a.employment.active);a.place=7;w.command_for_test(4,Method::ApplyJob,7);w.run_seconds(130);
 CHECK(w.state().actors[3].employment.active);CHECK(w.state().actors[3].employment.organization==2);CHECK(w.state().actors[3].mind.career.hired==1);w.validate();
}
TEST("career_runtime",full_vacancy_cannot_create_phantom_hire){
 auto w=careers();w.edit_for_test().autonomy=false;w.edit_for_test().hiring[1].capacity=0;w.run_seconds(9*3600);
 w.edit_for_test().actors[3].place=7;w.command_for_test(4,Method::ApplyJob,7);w.run_seconds(130);
 CHECK(!w.state().actors[3].employment.active);CHECK(w.state().actors[3].mind.career.refused==1);w.validate();
}
TEST("career_runtime",same_salary_restatement_does_not_repeat_reviews){
 auto w=careers();auto k=complete_offer();w.career_information_for_test(4,k,900000);w.run_seconds(180);
 auto before=w.state().actors[3].mind.career.semantic_changes;
 w.career_information_for_test(4,k,900001);CHECK(w.state().actors[3].mind.career.semantic_changes==before);
}
TEST("career_runtime",career_review_consumes_cognition_and_keeps_current_action){
 auto w=careers();w.career_information_for_test(4,complete_offer(),900000);w.run_seconds(180);
 const auto& a=w.state().actors[3];CHECK(a.mind.career.questions_reviewed>0);CHECK(a.cog.completed_ops>=2);CHECK(a.mind.career.active_target);
}
TEST("career_runtime",save_restore_mid_career_review){
 auto w=careers();bool found=false;
 for(unsigned i=0;i<8000;++i){w.run_ms(50);if(w.state().actors[3].cog.operation==Operation::CompareCareerFacts){found=true;break;}}
 CHECK(found);auto p=std::filesystem::temp_directory_path()/"life-career-stage.bin";w.save(p.string());auto r=World::load(p.string());
 w.run_seconds(600);r.run_seconds(600);CHECK(w.hash()==r.hash());std::filesystem::remove(p);
}
TEST("career_runtime",knowledge_message_only_inquiry_is_not_employment){
 auto w=careers();auto k=complete_offer();k.known=LocationKnown|WageKnown;w.career_information_for_test(4,k,900000);w.run_seconds(300);
 CHECK(!w.state().actors[3].employment.active);
}
TEST("career_runtime",new_salary_information_changes_actual_employment_not_hidden_world) {
 auto a=careers(),b=careers();
 for(auto* w:{&a,&b}){
  w->edit_for_test().autonomy=false;w->run_seconds(9*3600);
  w->edit_for_test().hiring[1].hourly=40;
  for(auto& person:w->edit_for_test().actors){person.mind.career.enabled=false;person.body.energy=person.body.water=.9;person.body.sleep=.1;}
  auto& target=w->edit_for_test().actors[1];target.mind.career.enabled=true;
  for(auto& q:target.mind.career.questions)q.stage=CareerStage::Deferred;
  auto k=complete_offer();k.hourly=10;w->career_information_for_test(2,k,900000);
  w->edit_for_test().autonomy=true;
 }
 auto k=complete_offer();k.hourly=40;b.career_information_for_test(2,k,900001);
 a.run_seconds(3*3600);b.run_seconds(3*3600);
 CHECK(a.state().actors[1].employment.organization==1);
 if(b.state().actors[1].employment.organization!=2){const auto& x=b.state().actors[1];std::cerr<<b.career_report_json()<<"\n";
  std::cerr<<"STATE "<<x.alive<<" "<<x.body.fatigue<<" "<<x.body.damage<<" "<<x.body.oxygen<<" "<<x.cog.active<<" "<<x.cog.focus<<" "<<x.cog.completed_ops<<" "<<x.cog.review_at<<" "<<unsigned(x.action.phase)<<"\n";
  for(const auto& t:x.cog.recent)std::cerr<<thought_json(t)<<"\n";
 }
 CHECK(b.state().actors[1].employment.organization==2);
 CHECK(b.state().actors[1].mind.career.hired==1);
 CHECK(b.state().actors[1].mind.career.questions_reviewed>0);
}

#else
TEST("career_runtime",real_information_to_employment_pipeline_required){CHECK(false && "No runtime career pipeline");}
#endif
