#include "test.hpp"
#include "life/civil.hpp"
using namespace life;
TEST("civil_unit", wear_is_time_based_and_composable){NEAR(worn_condition(worn_condition(.8,.02,3600),.02,3600),worn_condition(.8,.02,7200),1e-12);NEAR(worn_condition(.8,.02,0),.8,0);THROWS(worn_condition(.8,.02,-1));}
TEST("civil_unit", chosen_goals_change_financial_reserve){CivilMemory a;a.enabled=true;a.garment_condition=.9;a.desired_tier=0;a.garment_tier=0;auto before=civil_budget(a,0,2);a.garment_condition=.2;auto after=civil_budget(a,0,2);CHECK(after.target>before.target);CHECK(after.protected_cash==before.protected_cash);a.desired_tier=2;CHECK(civil_budget(a,0,2).target>after.target);}
TEST("civil_unit", budget_does_not_invent_income){CivilMemory a;a.enabled=true;a.garment_condition=.2;CHECK(civil_budget(a,0,0).pressure>0);CHECK(civil_budget(a,1000,0).pressure==0);}
TEST("civil_unit", phone_address_requires_explicit_source){CivilMemory m;CHECK(!m.number_for(2));CHECK(m.learn_number(2,12345,99,0));CHECK(*m.number_for(2)==12345);CHECK(!m.learn_number(2,12345,99,0));THROWS(m.learn_number(2,0,100,0));}
TEST("civil_unit", procedure_lesson_is_not_practice){ProjectMemory p;p.enabled=true;LessonMemory m;ProcedureLesson l;l.root=99;l.teacher=2;l.procedure=familiar_procedure(GoalKind::FindCompany,12);l.procedure.successes=200;auto result=learn_procedure(p,m,l,0xffff,.9,0);CHECK(result);CHECK(p.known(l.procedure.id));NEAR(p.known(l.procedure.id)->successes,1,0);NEAR(p.known(l.procedure.id)->failures,1,0);CHECK(p.known(l.procedure.id)->source==99);}
TEST("civil_unit", instruction_with_unknown_step_cannot_be_executed){ProjectMemory p;p.enabled=true;LessonMemory m;ProcedureLesson l;l.root=99;l.teacher=2;l.procedure=familiar_procedure(GoalKind::FindCompany,12);CHECK(learn_procedure(p,m,l,1u<<unsigned(StepKind::Visit),.9,0));CHECK(!p.known(l.procedure.id));CHECK(m.lessons.size()==1);CHECK(m.lessons[0].missing_steps!=0);}
TEST("civil_unit", repeating_lesson_does_not_grant_mastery){ProjectMemory p;p.enabled=true;LessonMemory m;ProcedureLesson l;l.root=99;l.teacher=2;l.procedure=familiar_procedure(GoalKind::FindCompany,12);CHECK(learn_procedure(p,m,l,0xffff,.9,0));auto mastery=p.known(1)->mastery;CHECK(!learn_procedure(p,m,l,0xffff,1,100));NEAR(p.known(1)->mastery,mastery,0);}
TEST("civil_unit", malformed_lesson_is_rejected){ProjectMemory p;p.enabled=true;LessonMemory m;ProcedureLesson l;l.root=99;l.teacher=2;l.procedure=familiar_procedure(GoalKind::FindCompany,12);l.procedure.steps[0].success=100;THROWS(learn_procedure(p,m,l,0xffff,1,0));}
TEST("civil_unit", two_stores_are_bindings_not_two_graphs){CivilMemory m;m.shops.push_back({3,99,0,0,{{1,35,0}}});m.shops.push_back({4,100,0,0,{{1,30,0}}});CHECK(m.shops.size()==2);CHECK(m.lessons.lessons.empty());}
TEST("civil_unit", future_essentials_respect_known_weekend_pay_gap){CivilMemory m;auto weekday=civil_budget(m,100,0,1*86400000+18*3600000);auto friday=civil_budget(m,100,0,4*86400000+18*3600000);CHECK(friday.protected_cash>weekday.protected_cash);CHECK(friday.pressure>weekday.pressure);}
TEST("civil_unit", discretionary_purchase_cannot_consume_precautionary_food_buffer){
 CivilMemory m;m.enabled=true;m.garment_tier=0;m.desired_tier=1;m.garment_condition=.7;
 auto b=civil_budget(m,150,0,4*86400000+16*3600000);
 CHECK(b.protected_cash>=3*(18+3*12));
 CHECK(150-b.protected_cash<95);
}
