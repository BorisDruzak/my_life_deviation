#include "test.hpp"
#include "life/numeric.hpp"
using namespace life;
TEST("numeric", body_loses_water) { Body b; advance_body(b,Biology{},Input{},Emotions{},3600); CHECK(b.water<.8); }
TEST("numeric", body_food_rate) { Body b; Input in;in.safety=0;auto old=b;advance_body(b,Biology{},in,Emotions{},3600);NEAR(b.energy,old.energy-.035*(1+.15*old.activation),1e-14); }
TEST("numeric", zero_time_exact) { Body b;auto old=b;advance_body(b,Biology{},Input{},Emotions{},0); CHECK(b.water==old.water); CHECK(approach(.37,.13,0,20)==.37); }
TEST("numeric", invalid_time_and_nan) { THROWS(approach(.1,.2,-1,30));THROWS(approach(.1,.2,0,0));THROWS(approach(NAN,.2,0,30)); Body b;THROWS(advance_body(b,Biology{},Input{},Emotions{},-1)); }
TEST("numeric", sleeping_restores_sleep_not_food) { Body b;b.sleep=.7;Input in;in.asleep=true;advance_body(b,Biology{},in,Emotions{},3600);CHECK(b.sleep<.7);CHECK(b.energy<.8); }
TEST("numeric", lack_of_oxygen_limits_cognition) { Body b;b.oxygen=.96;auto c=capability(b,Cognitive{},false);CHECK(c.gate==0);CHECK(c.operations==0);CHECK(c.context==0); }
TEST("numeric", zero_signal_no_hallucination) { Body b;b.energy=1;b.water=1;b.sleep=0;b.fatigue=0;auto s=signals(b,Biology{});for(double x:s)CHECK(x==0); CHECK(sensation(0,0,1)==0); }
TEST("numeric", cognition_does_not_rewrite_baseline) { Body b;b.sleep=.9;Cognitive c;auto r=capability(b,c,false);CHECK(r.current[1]<c.base[1]);CHECK(c.base[1]==.5); }
TEST("numeric", sleep_has_no_conscious_budget) { auto c=capability(Body{},Cognitive{},true);CHECK(c.gate==0&&c.context==0&&c.operations==0); }
TEST("numeric", pleasant_familiar_activity) { double p=pleasantness(.4,0,0,0,0,0,1,.8);CHECK(p>0); CHECK(saturation(.2,.8*p,0,.06,1)>.2); }
TEST("numeric", bad_experience_never_becomes_pleasant) {CHECK(pleasantness(-.3,0,0,0,0,0,1,1)<0);}
TEST("numeric", saturation_constant_input_split) {double x=saturation(.3,.8,.2,.06,3);double y=saturation(saturation(.3,.8,.2,.06,1),.8,.2,.06,2);NEAR(x,y,1e-14);}
TEST("numeric", habituation_recovers) {CHECK(habituation(.6,0,1)<.6);CHECK(habituation(.6,1,1)>.6);}
TEST("numeric", desire_accumulates_and_saturates) {Desire d;double old=d.deficit;for(int i=0;i<100;++i)d.advance(3600,0,0);CHECK(d.deficit>old&&d.deficit<=1);}
TEST("numeric", desire_result_once_even_out_of_order) {Desire d;d.deficit=.8;d.refractory=.2;CHECK(d.satisfy(2,.6));NEAR(d.deficit,.32,1e-14);NEAR(d.refractory,.68,1e-14);CHECK(!d.satisfy(2,.6));CHECK(d.satisfy(1,.2));CHECK(!d.satisfy(1,.2));}
TEST("numeric", desire_trigger_without_satisfaction) {Desire a,b; a.advance(1,0,0);b.advance(1,.4,0);CHECK(b.value>a.value);CHECK(b.deficit==a.deficit);}
TEST("numeric", desire_does_not_damage_body) {Body body;Desire d;d.deficit=1;d.advance(3600,.5,0);CHECK(body.damage==0);}
TEST("numeric", desire_constant_dr_split) {Desire a,b;a.advance(7200,0,0);b.advance(3600,0,0);b.advance(3600,0,0);NEAR(a.deficit,b.deficit,1e-14);NEAR(a.refractory,b.refractory,1e-14);}
TEST("numeric", appraisal_needs_cause) {Appraisal a;Body b;b.sleep=1;b.pain=1;for(double x:appraisal_targets(a,b))CHECK(x==0);}
TEST("numeric", affect_expires_not_instantly_erased) {Affect a;Emotions t{};t[0]=1;a.set(1,1000,t);a.advance(0,1000);CHECK(a.total[0]>.6);a.advance(1000,1000);CHECK(a.total[0]>0&&a.total[0]<.633);}
TEST("numeric", affect_duplicate_cause_does_not_sum) {Affect a;Emotions t{};t[0]=.2;for(int i=0;i<100;++i)a.set(1,1000,t);CHECK(a.causes.size()==1);a.advance(0,1000);CHECK(a.total[0]<.2);}
TEST("numeric", attention_switch_not_restart) {Attention a;auto c=capability(Body{},Cognitive{},false);CHECK(a.select(1,0,.9,false,0,c,.5,1));auto due=a.ready;CHECK(!a.select(2,0,1,false,1,c,.5,1));CHECK(a.ready==due);}
TEST("numeric", desire_not_emergency_attention) {Attention a;auto c=capability(Body{},Cognitive{},false);a.focus=1;a.since=10000;CHECK(!a.select(9,0,1,false,10001,c,.5,1));}
