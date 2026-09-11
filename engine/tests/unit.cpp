#include "mld/internal.hpp"
#include "check.hpp"
using namespace mld;
int main(){
    test("relax actual transition",[]{near(relax(.2,.8,30,30),.8+(.2-.8)*std::exp(-1));});
    test("zero time bit-preserving",[]{check(relax(.3,.8,0,30)==.3);});
    test("reject invalid even at zero dt",[]{rejects([]{relax(.3,.8,0,0);});rejects([]{clamp(NAN);});});

    test("body rates from one snapshot",[]{Body b; auto n=advance_body(b,{}, {},{},3600);near(n.energy,.8-.035*1.015);near(n.water,.8-.045*1.015);near(n.sleep,.195);});
    test("rest is not sleep",[]{Body b;PhysicalInput i;i.asleep=true;check(advance_body(b,{},i,{},3600).sleep<b.sleep);check(advance_body(b,{}, {},{},3600).sleep>b.sleep);});
    test("zero body dt",[]{Body b;auto n=advance_body(b,{}, {},{},0);check(n.energy==b.energy&&n.activation==b.activation);});
    test("oxygen damages without desire",[]{Body b;b.oxygen=1;PhysicalInput i;i.oxygen=1;check(advance_body(b,{},i,{},60).damage>b.damage);});
    test("normal capacities",[]{Body b;b.fatigue=0;auto c=capacities(b,{});for(double x:c)near(x,.5);check(context_slots(c)==7&&operations(c)==18);});
    test("no cognition asleep",[]{auto c=capacities({}, {},true);for(double x:c)near(x,0);check(context_slots(c,false)==0);});
    test("no invented emotion",[]{for(auto x:emotion_targets({}))near(x,0);});
    test("fear appraisal",[]{Appraisal a;a.significance=.8;a.harm=.7;a.near=.9;a.control=.2;a.uncertain=.6;near(emotion_targets(a)[0],.43848);});
    test("novelty not needed for enjoyment",[]{check(pleasure(.3,.1,0,0,0,1,.9,.4,0)>0);});
    test("exact reservoir composition",[]{near(reservoir(reservoir(.7,.2,.1,.06,1),.2,.1,.06,2),reservoir(.7,.2,.1,.06,3));});
    test("desire deficit accumulates",[]{auto d=advance_desire({},0,0,72*3600);near(d.deficit,.625);});
    test("satisfy different from seeing",[]{Desire d;d.deficit=.8;d.satiation=.2;satisfy(d,.6);near(d.deficit,.32);near(d.satiation,.68);});
    test("desire zero dt",[]{Desire d;check(advance_desire(d,.4,0,0).value==d.value);rejects([&]{advance_desire(d,0,0,-1);});});
    test("contextual trigger",[]{auto a=advance_desire({},.4,0,10);auto b=advance_desire({},0,0,10);check(a.value>b.value);});
    test("associative not causal",[]{Stats s;learn_association(s,.8,.5,1,.5,1);check(s.mean>0);});
    test("unobserved is not zero",[]{Stats s;s.mean=.5;learn_association(s,{},.5,1,.5,1);near(s.mean,.5);});
    test("no conscious experience",[]{Stats s;learn_association(s,.8,.5,1,.5,1,false);near(s.count,0);});
    test("sparse channel specific",[]{Reactions w;std::array<std::optional<double>,6> y{};y[0]=.6;check(w.learn(1,{{1,1}},y,.5,1,0)==1);check(w.effects({{1,1}},0)[0]>0);near(w.effects({{1,1}},0)[5],0);});
    test("duplicate episode",[]{Reactions w;std::array<std::optional<double>,6> y{};y[5]=.4;w.learn(1,{{1,1}},y,.5,1,0);auto old=w.weights[0].value;w.learn(1,{{1,1}},y,.5,1,0);near(old,w.weights[0].value);});
    test("four new pairs maximum",[]{Reactions w;std::array<std::optional<double>,6> y{};y.fill(.5);check(w.learn(1,{{1,1},{2,.9},{3,.8}},y,1,1,0)==4);});
    test("unknown permission not false",[]{Rule r{1,{{2,Truth::False}},3,1};int budget=10;check(!infer(r,{},4,budget));check(budget<10);});
    test("rule proof respects confidence",[]{Rule r{1,{{2,Truth::True}},3,.7};int budget=10;auto f=infer(r,{{2,4,Truth::True,.6,1}},4,budget);check(f.has_value());near(f->confidence,.6);});
    test("contradiction not truth",[]{Rule r{1,{{2,Truth::True}},3,1};int budget=10;check(!infer(r,{{2,4,Truth::Conflict,.9,1}},4,budget));});
    test("taboo is finite",[]{near(moral_cost(1,1),1);});
    test("subjective penalty example",[]{near(expected_sanction(.4,.8,.5,.6),.096);});
    test("dependence partial knowledge",[]{auto d=dependence({{1,.8,.25},{1,.4,std::nullopt}});near(*d.value,.6);near(d.coverage,.5);});
    test("dependence .35 example",[]{near(*dependence({{1,.8,.25},{1,.4,.75}}).value,.35);});
    test("conflicting witnesses are not silently resolved",[]{Rule r{1,{{2,Truth::True}},3,1};int budget=10;check(!infer(r,{{2,4,Truth::True,.8,1},{2,4,Truth::False,.8,2}},4,budget));});
    test("W global shared capacity",[]{Reactions w;std::array<std::optional<double>,6> y{};y[0]=.5;check(w.learn(1,{{3,1}},y,.5,1,0,512)==0);check(w.weights.empty());});
    test("W empty primary cannot self reinforce",[]{Reactions w;std::array<std::optional<double>,6> y{};y[0]=.5;w.learn(1,{{3,1}},y,.5,1,0);auto old=w.weights.front().value;y.fill(std::nullopt);w.learn(2,{{3,1}},y,.5,1,0);near(w.weights.front().value,old);});
    test("W indexed update leaves unrelated properties intact",[]{Reactions w;for(Id i=0;i<100;++i)w.weights.push_back({i,0,.1,0,0});std::array<std::optional<double>,6> y{};y[0]=.6;w.learn(1,{{2,1},{3,.5}},y,.5,1,0);check(w.weights[2].value>.1);near(w.weights[50].value,.1);});
    test("zero sanction no monetary side effect",[]{near(expected_sanction(0,.8,.5,.6),0);});
    return result();
}
