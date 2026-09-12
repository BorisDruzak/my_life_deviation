#include "test.hpp"
#include "life/mind.hpp"
using namespace life;
static Episode sample(std::uint64_t id){Episode e;e.id=id;e.primary_observed=1;e.observed=1u<<std::uint8_t(Metric::Pleasantness);e.primary[0]=.5;e.outcome[10]=.6;e.features={{1,1,1,1},{2,1,1,.8}};return e;}
TEST("mind", association_learns_without_causal_thinking){Estimate q;q.observe(.8,.5,1,1,0,true);CHECK(q.mean>0&&q.count==1);}
TEST("mind", no_consciousness_no_association){Estimate q;q.observe(.8,.5,1,1,.5,false);CHECK(q.mean==0&&q.count==0);}
TEST("mind", primary_result_not_copied_to_other_channels){Learning l;Cognitive c;l.observe(sample(1),c,.5);for(auto w:l.weights)CHECK(w.channel==0);}
TEST("mind", episode_not_replayed){Learning l;Cognitive c;CHECK(l.observe(sample(1),c,.5));auto w=l.weights;CHECK(!l.observe(sample(1),c,.5));CHECK(l.episodes==1);CHECK(l.weights[0].value==w[0].value);}
TEST("mind", primary_zero_extinguishes_acquired_response){Learning l;Cognitive c;for(int i=1;i<50;++i)l.observe(sample(i),c,.5);auto before=l.effects(sample(50).features,0)[0];for(int i=50;i<100;++i){auto e=sample(i);e.primary[0]=0;l.observe(e,c,.5);}CHECK(l.effects(sample(100).features,0)[0]<before);}
TEST("mind", weights_use_joint_error){Learning l;Cognitive c;l.observe(sample(1),c,.5);CHECK(l.weights.size()==2);NEAR(l.weights[0].value,.0025,1e-14);NEAR(l.weights[1].value,.0025,1e-14);}
TEST("mind", learning_respects_caps){Learning l;Cognitive c;for(int i=1;i<300;++i){auto e=sample(i);e.features={{Id(i*2),1,1,1},{Id(i*2+1),1,1,.5}};e.primary_observed=63;e.primary.fill(.8);l.observe(e,c,.5);CHECK(l.last_created<=4);CHECK(l.last_considered<=96);CHECK(l.weights.size()<=512);} }
TEST("mind", sexual_channel_needs_own_primary_result){Learning l;Cognitive c;auto e=sample(1);l.observe(e,c,.5);CHECK(l.effects(e.features,0)[5]==0);e.id=2;e.primary_observed=32;e.primary[5]=.7;l.observe(e,c,.5);CHECK(l.effects(e.features,0)[5]>0);}
TEST("mind", missing_factor_is_not_observed_absence){CausalHypothesis c;CausalSample y{1,true,true,true,true,.8,1,{1,2}},n{2,true,false,false,true,.1,1,{1,2}};auto caps=capability(Body{},Cognitive{},false);Budget b(caps);CHECK(!c.compare(y,n,.5,.5,2,b));CHECK(c.weight==0);n.factor_observed=true;CHECK(c.compare(y,n,.5,.5,2,b));CHECK(c.effect>0);CHECK(!c.compare(y,n,.5,.5,2,b));}
TEST("mind", empty_context_not_perfect_causal_comparison){CausalHypothesis c;CausalSample y{1,true,true,true,true,.8,1,{}},n{2,true,true,false,true,.1,1,{}};Budget b(capability(Body{},Cognitive{},false));CHECK(!c.compare(y,n,.5,.5,2,b));}
TEST("mind", unknown_replacement_not_zero){auto d=dependency({{1,.8,std::nullopt},{1,.4,.75}});CHECK(d.value.has_value());NEAR(*d.value,.1,1e-14);NEAR(d.coverage,.5,1e-14);auto u=dependency({{1,.8,std::nullopt}});CHECK(!u.value);}
TEST("mind", dependency_numeric_example){auto d=dependency({{1,.8,.25},{1,.4,.75}});CHECK(d.value.has_value());NEAR(*d.value,.35,1e-14);}
TEST("mind", finite_taboo_and_deduplicated_aspects){NEAR(moral_cost({{1,1,1}}),1,0);NEAR(moral_cost({{1,.5,1},{1,.5,1}}),.5,0);}
TEST("mind", subjective_sanction_example){NEAR(sanction(.4,.8,.5,.6),.096,1e-14);}
TEST("mind", norm_changes_only_with_new_basis){NEAR(revise_norm(.8,.2,.5,1,1),.7940299002495008,1e-14);CHECK(revise_norm(.8,.2,.5,0,1)==.8);}
TEST("mind", duplicate_rumor_known_and_unknown_origins){Trust t;CHECK(t.receive({1,7,0,1}));auto p=t.expectation();CHECK(!t.receive({1,7,0,1}));CHECK(t.receive({2,7,0,1}));CHECK(t.expectation()==p);Trust u;u.receive({1,0,0,1});p=u.expectation();u.receive({2,0,0,1});CHECK(u.expectation()==p);}
TEST("mind", knowledge_requires_content_and_prerequisites){std::vector<Rule> rules{{1,1,1,{}},{2,1,2,{1}}};Knowledge k;k.learn(rules[1],rules,1,1,1,1);CHECK(k.get(2)<.6);CHECK(k.depth(1,rules)==0);for(int i=0;i<10;++i)k.learn(rules[0],rules,1,1,1,1);CHECK(k.depth(1,rules)==1);}
TEST("mind", budget_is_shared){auto c=capability(Body{},Cognitive{},false);Budget b(c);CHECK(b.hold(c.context));CHECK(!b.hold());CHECK(b.pay(c.operations));CHECK(!b.pay());}
static PersonalView decision_fixture(){PersonalView v;v.self=1;v.home=v.place=1;v.capability=capability(Body{},Cognitive{},false);v.need[0]=.9;v.food=1;v.known[1]=true;v.forecasts[1][0]=.5;v.places={{1,service(Method::Eat),Truth::Unknown,0,1}};return v;}
TEST("mind", planner_uses_only_personal_view){auto v=decision_fixture();Seed s{42,"0.7.0",catalogue_hash()};auto a=Planner::choose(v,s),b=Planner::choose(v,s);CHECK(a.method==Method::Eat&&a.method==b.method);CHECK(a.score==b.score);CHECK(a.operations<=v.capability.operations);CHECK(a.peak_slots<=v.capability.context);}
TEST("mind", full_resource_prevents_useless_consumption){auto v=decision_fixture();v.need.fill(0);Seed s{42,"0.7.0",catalogue_hash()};CHECK(Planner::choose(v,s).method==Method::Idle);}
TEST("mind", prepared_effects_equal_reference){
 Learning l;Cognitive c;Episode e;e.id=1;e.primary[0]=.4;e.primary[5]=.3;e.primary_observed=33;e.features={{11,1,1,.9},{12,1,.5,.8},{11,1,.4,.6}};
 l.observe(e,c,.5);auto p=l.prepare(e.features);
 for(Tick t:{0ll,1000ll,17000ll,86400000ll})CHECK(l.effects(e.features,t)==l.prepared_effects(p,t));
 e.id=2;e.primary[0]=-.2;l.observe(e,c,.5);p=l.prepare(e.features);CHECK(l.effects(e.features,9000)==l.prepared_effects(p,9000));
}
