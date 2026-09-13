#include "test.hpp"
#include "life/mind.hpp"
#include "life/activity_resources.hpp"
#include "life/employment.hpp"
using namespace life;
TEST("life_contracts",work_and_talk_share_resources_with_a_cost){auto f=combine_resources(activity_resources(Method::Work),activity_resources(Method::Talk));CHECK(f.allowed);CHECK(f.efficiency>0&&f.efficiency<1);}
TEST("life_contracts",sleep_cannot_run_with_conversation){CHECK(!combine_resources(activity_resources(Method::Sleep,false,true),activity_resources(Method::Talk)).allowed);}
TEST("life_contracts",walking_and_talking_are_compatible){CHECK(combine_resources(activity_resources(Method::Idle,true),activity_resources(Method::Talk)).allowed);}
TEST("life_contracts",two_simultaneous_speaking_activities_are_not_allowed){CHECK(!combine_resources(activity_resources(Method::Talk),activity_resources(Method::Talk)).allowed);}
TEST("life_contracts",negative_or_nan_resource_load_is_rejected){ActivityResources a,b;a.load[0]=-.1;THROWS(combine_resources(a,b));a.load[0]=std::nan("");THROWS(combine_resources(a,b));}
TEST("life_contracts",workday_is_an_eight_hour_known_window){EmploymentContract c;c.active=true;CHECK(!c.in_shift(8*3600000LL));CHECK(c.in_shift(9*3600000LL));CHECK(c.in_shift(16*3600000LL));CHECK(!c.in_shift(17*3600000LL));CHECK(!c.in_shift(5*86400000LL+10*3600000LL));}
TEST("life_contracts",supervisor_request_changes_a_motive_not_an_action){EmploymentContract c;c.active=true;c.supervisor=3;auto before=c.obligation(10*3600000LL);CHECK(c.receive_request(10,3,.8,10*3600000LL));CHECK(c.obligation(10*3600000LL)>before);CHECK(!c.receive_request(10,3,.8,10*3600000LL));}
TEST("life_contracts",stranger_does_not_gain_supervisor_authority){EmploymentContract c;c.active=true;c.supervisor=3;CHECK(!c.receive_request(10,2,.8,0));CHECK(c.last_request==0);}
TEST("life_contracts",work_contribution_is_idempotent){Organization o;o.members={{1,10,1,5},{2,12,1,5}};CHECK(o.contribute(1,7,1,.8));CHECK(!o.contribute(1,7,1,.8));NEAR(o.members[0].hours,1,0);NEAR(o.members[0].effective_hours,.8,1e-12);}
TEST("life_contracts",one_employee_cannot_raise_everyones_wage_alone){Organization o;o.members={{1,10,1,5},{2,12,1,5}};CHECK(o.contribute(1,7,6,1));auto paid=o.settle(0);CHECK(paid.size()==2);NEAR(o.development,0,0);CHECK(o.level==0);NEAR(paid[0].amount,60,1e-12);NEAR(paid[1].amount,0,0);}
TEST("life_contracts",collective_contribution_eventually_raises_everyones_wage){Organization o;o.members={{1,10,1,5},{2,12,1.5,5}};for(int day=0;day<5;++day){CHECK(o.contribute(1,10+2*day,5,1));CHECK(o.contribute(2,11+2*day,5,1));auto paid=o.settle(day);CHECK(paid.size()==2);}CHECK(o.level==1);CHECK(o.multiplier()>1);CHECK(o.contribute(1,30,5,1));CHECK(o.contribute(2,31,5,1));auto paid=o.settle(5);CHECK(paid[0].amount>50&&paid[1].amount>60);}
TEST("life_contracts",repeated_payroll_cannot_create_money){Organization o;o.members={{1,10,1,5}};CHECK(o.contribute(1,1,2,1));CHECK(o.settle(0).size()==1);CHECK(o.settle(0).empty());CHECK(o.payroll.size()==1);NEAR(o.members[0].paid,20,1e-12);}
TEST("life_contracts",invalid_work_contribution_is_rejected){Organization o;o.members={{1,10,1,5}};THROWS(o.contribute(1,1,-1,1));THROWS(o.contribute(1,1,1,2));THROWS(o.contribute(99,1,1,1));}
