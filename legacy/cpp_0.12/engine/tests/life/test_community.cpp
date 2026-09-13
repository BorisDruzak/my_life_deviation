#include "test.hpp"
#include "life/world.hpp"
using namespace life;
TEST("community",budget_four_can_choose) {
 auto w=World::generate(42,8); auto& a=w.edit_for_test().actors[0];
 a.mind.cognition.base[4]=0; a.body.energy=.35; a.food=2;
 a.mind.believed_food=2; ++w.edit_for_test().ledger.initial_food; a.mind.social.enabled=false;
 a.mind.known.fill(false); a.mind.known[std::size_t(Method::Eat)]=true;
 a.leisure=a.social=1; a.desire.deficit=0;
 w.run_seconds(600);
 CHECK(w.state().actors[0].completed[std::size_t(Method::Eat)]>0);
}
#include "life/community.hpp"
Information example_news(std::uint64_t id=10) {
 Information f;f.id=id;f.subject=2;f.location=3;f.occurred_at=0;f.importance=.5;
 return f;
}
TEST("community_unit",memory_half_life) {MemoryDetail d{1,2,0};NEAR(d.at(7200000),.5,1e-12);}
TEST("community_unit",read_does_not_reinforce) {MemoryDetail d{1,2,0};auto a=d.at(7200000);for(int i=0;i<30;++i)d.at(7200000);NEAR(a,d.at(7200000),0);}
TEST("community_unit",negative_memory_time_rejected) {MemoryDetail d{1,2,100};THROWS(d.at(0));}
TEST("community_unit",delivery_idempotent) {CommunityMemory m;CHECK(m.receive(example_news(),1,50,100));CHECK(!m.receive(example_news(),1,50,200));CHECK(m.received==1);}
TEST("community_unit",late_delivery_not_time_reversal) {CommunityMemory m;auto f=example_news();CHECK(m.receive(f,1,50,100000));CHECK(m.recall(10,100001).has_value());}
TEST("community_unit",event_in_future_rejected) {CommunityMemory m;auto f=example_news();f.occurred_at=1000;THROWS(m.receive(f,1,50,1));}
TEST("community_unit",detail_fades_before_gist) {CommunityMemory m;m.receive(example_news(),1,50,0);auto f=m.recall(10,12*3600000LL);CHECK(f.has_value());CHECK(f->location==0);}
TEST("community_unit",forgetting_keeps_delivery_protection) {CommunityMemory m;m.receive(example_news(),1,50,0);m.forget(1000LL*3600000);CHECK(m.entries.empty());CHECK(!m.receive(example_news(),1,50,1000LL*3600000));}
TEST("community_unit",no_reputation_without_evidence) {CommunityMemory m;CHECK(!m.opinion(2,0).material);CHECK(!m.opinion(2,0).honesty);}
TEST("community_unit",job_evidence_changes_material_not_honesty) {CommunityMemory m;auto f=example_news();f.kind=NewsKind::Employment;f.material=.8;m.receive(f,2,50,0);CHECK(m.opinion(2,0).material.has_value());CHECK(!m.opinion(2,0).honesty);}
TEST("community_unit",walking_is_not_romance_or_badness) {CommunityMemory m;m.receive(example_news(),1,50,0);CHECK(!m.opinion(2,0).honesty);CHECK(!m.opinion(2,0).helpfulness);}
TEST("community_unit",private_information_finite_cost) {CommunityMemory m;auto f=example_news();f.disclosure=Disclosure::Entrusted;f.sensitivity=.8;auto c=m.disclosure_cost(f);CHECK(c>0&&std::isfinite(c));}
TEST("community_unit",open_not_globally_known) {CommunityMemory a,b;a.receive(example_news(),1,50,0);CHECK(!b.recall(10,0));}
TEST("community_unit",confidentiality_changes_price) {CommunityMemory a,b;a.confidentiality=0;b.confidentiality=1;auto f=example_news();f.disclosure=Disclosure::Entrusted;CHECK(a.disclosure_cost(f)<b.disclosure_cost(f));}
TEST("community_unit",same_claim_not_independent_evidence) {CommunityMemory m;m.receive(example_news(),1,50,0);m.receive(example_news(),3,51,100);CHECK(m.entries.size()==1);}
TEST("community_unit",source_forgetting_not_firsthand) {CommunityMemory m;auto f=example_news();m.receive(f,3,50,0);CHECK(m.entries.size()==1);m.entries[0].source.half_life_hours=.001;auto r=m.recall(10,3600000);CHECK(r);CHECK(r->origin==NewsOrigin::Reported);CHECK(r->cited_source==0);}
TEST("community_unit",candidate_bound) {CommunityMemory m;for(unsigned i=1;i<100;++i)m.receive(example_news(i),1,100+i,0);CHECK(m.candidates(0,16).size()==16);}
TEST("community_unit",rehearsal_not_truth_update) {CommunityMemory m;m.receive(example_news(),1,50,0);CHECK(m.recall(10,0));auto c=m.recall(10,0)->confidence;m.rehearse(10,3600000);NEAR(m.recall(10,3600000)->confidence,c,0);}
TEST("community_unit",job_rates_increasing) {auto j=job_catalogue();CHECK(j.size()==4);for(int i=1;i<4;++i)CHECK(j[i].hourly>j[i-1].hourly);}
TEST("community_unit",free_and_paid_recreation) {auto r=recreation_catalogue();CHECK(r[0].fee==0);CHECK(r[5].fee>r[1].fee);}
TEST("community_unit",appearance_preferences_not_uniform) {Appearance a,b;a.height_cm=160;b.height_cm=195;Preference p;p.preferred_height=160;CHECK(attraction(a,p)>attraction(b,p));}
TEST("community",profile_has_four_jobs) {auto w=World::generate(42,8);w.configure_community();unsigned jobs=0;for(const auto& p:w.state().places)jobs+=p.job_level>=0;CHECK(jobs==4);}
TEST("community",profile_has_six_leisure_variants) {auto w=World::generate(42,8);w.configure_community();unsigned count=0;for(const auto& p:w.state().places)count+=p.recreation>=0;CHECK(count==6);}
TEST("community",community_is_real_serialized_state) {auto w=World::generate(42,8);w.configure_community();CHECK(w.state().community.enabled);w.save("community-test.save");auto r=World::load("community-test.save");CHECK(r.hash()==w.hash());}
TEST("community",appearance_is_seeded) {auto a=World::generate(42,8),b=World::generate(42,8);a.configure_community();b.configure_community();CHECK(a.hash()==b.hash());CHECK(a.state().actors[0].appearance.height_cm!=a.state().actors[1].appearance.height_cm);}
TEST("community",initial_information_is_not_all_to_all) {auto w=World::generate(42,8);w.configure_community();CHECK(w.state().actors[0].mind.social.community.entries.size()>0);CHECK(w.state().actors[0].mind.social.community.entries.size()<20);}
TEST("community",community_costs_known_not_free) {auto w=World::generate(42,8);w.configure_community();auto p=w.personal_view(1);CHECK(p.economy.enabled);CHECK(p.places[2].price>1);}
#include <filesystem>
#include <algorithm>
namespace {
World information_room(bool group=false){
 auto w=World::generate(42,8);w.configure_community();auto& s=w.edit_for_test();s.autonomy=false;
 for(auto& a:s.actors){a.mind.social.enabled=a.id<=(group?4u:2u);a.mind.social.methods.fill(MethodBelief{});a.mind.social.community.entries.clear();a.mind.social.community.shared.clear();a.social=.15;a.leisure=.9;
   a.mind.social.methods[std::size_t(Interaction::ShareNews)]={.9,.3,.95,.9,s.next_id++};
   if(a.mind.social.enabled){a.place=2;a.mind.known.fill(false);a.mind.known[std::size_t(Method::Talk)]=a.mind.known[std::size_t(Method::Social)]=true;}
 }
 w.command_for_test(1,Method::Talk,2);
 if(group){w.command_for_test(3,Method::Talk,2);for(unsigned n=2;n<4;++n){Percept p;p.token=1;p.since=p.last=0;p.exposure_id=10000+n;p.noticed=p.recognized=true;p.feature_work=1;s.actors[n].cog.percepts.push_back(p);}}
 s.autonomy=true;
 return w;
}
Id put_news(World& w,Disclosure privacy=Disclosure::Open){auto& s=w.edit_for_test();auto f=example_news(s.next_id++);f.subject=8;f.kind=NewsKind::Employment;f.material=.8;f.disclosure=privacy;f.importance=.8;f.cited_source=1;
 s.actors[0].mind.social.community.receive(f,0,s.next_id++,s.now);return Id(f.id);}
void at_work_time(World& w){auto& s=w.edit_for_test();s.now=9*3600000;s.next_physical=s.now;for(auto& a:s.actors){a.physical_at=s.now;a.cog.review_at=s.now;}}
}
TEST("community",message_does_not_transfer_before_completion){auto w=information_room();auto id=put_news(w);w.propose_social_for_test(1,Interaction::ShareNews,2,id);w.run_seconds(20);CHECK(!w.state().actors[1].mind.social.community.find(id));w.run_seconds(60);CHECK(w.state().actors[1].mind.social.community.find(id));}
TEST("community",heard_content_is_reported_not_world_truth){auto w=information_room();auto id=put_news(w);w.propose_social_for_test(1,Interaction::ShareNews,2,id);w.run_seconds(80);auto r=w.state().actors[1].mind.social.community.recall(id,w.state().now);CHECK(r);CHECK(r->origin==NewsOrigin::Reported);CHECK(r->cited_source==1);NEAR(r->material,.8,1e-12);}
TEST("community",group_listeners_receive_one_shared_utterance){auto w=information_room(true);auto id=put_news(w);w.propose_social_for_test(1,Interaction::ShareNews,2,id);w.run_seconds(80);CHECK(w.state().community.group_deliveries>=2);CHECK(w.state().actors[2].mind.social.community.find(id));CHECK(w.state().actors[3].mind.social.community.find(id));CHECK(!w.state().actors[4].mind.social.community.find(id));}
TEST("community",late_listener_cannot_hear_past_utterance){auto w=information_room();auto id=put_news(w);w.propose_social_for_test(1,Interaction::ShareNews,2,id);w.run_seconds(10);auto& s=w.edit_for_test();s.actors[2].place=s.actors[3].place=2;s.actors[2].mind.social.enabled=s.actors[3].mind.social.enabled=true;
 auto old=s.autonomy;s.autonomy=false;w.command_for_test(3,Method::Talk,2);s.autonomy=old;w.run_seconds(70);CHECK(!s.actors[2].mind.social.community.find(id));}
TEST("community",lost_listener_parent_invalidates_delivery){auto w=information_room(true);auto id=put_news(w);w.propose_social_for_test(1,Interaction::ShareNews,2,id);w.run_seconds(10);w.command_for_test(3,Method::Leisure,2);w.run_seconds(70);CHECK(!w.state().actors[2].mind.social.community.find(id));CHECK(w.state().community.missed>0);}
TEST("community",secret_cost_changes_autonomous_choice){auto low=information_room(),high=information_room();put_news(low,Disclosure::Entrusted);put_news(high,Disclosure::Entrusted);low.edit_for_test().actors[0].mind.social.community.confidentiality=0;high.edit_for_test().actors[0].mind.social.community.confidentiality=1;low.run_seconds(150);high.run_seconds(150);CHECK(low.state().community.disclosures>0);CHECK(high.state().community.disclosures==0);}
TEST("community",news_save_mid_speech_preserves_future){auto a=information_room(true);auto id=put_news(a);a.propose_social_for_test(1,Interaction::ShareNews,2,id);a.run_seconds(20);auto p=(std::filesystem::temp_directory_path()/"community-speech.save").string();a.save(p);auto b=World::load(p);a.run_seconds(100);b.run_seconds(100);CHECK(a.hash()==b.hash());std::filesystem::remove(p);}
TEST("community",community_worker_and_reference_equivalence){auto a=information_room(true);put_news(a);auto b=a;b.set_indexed(false);b.set_workers(4,1);a.run_seconds(180);b.run_seconds(180);CHECK(a.hash()==b.hash());}
TEST("community",community_logging_does_not_create_events){auto a=information_room(true);put_news(a);auto b=a;unsigned n=0;b.set_logger([&](const auto&){++n;});a.run_seconds(120);b.run_seconds(120);CHECK(n>0);CHECK(a.hash()==b.hash());}
TEST("community",known_kin_cost_is_finite_and_subjective){auto w=World::generate(42,8);w.configure_community();auto v=w.personal_view(1).social;v.memory.methods[1].mastery=.9;PersonBelief p;p.id=2;p.known_kin=false;v.familiarities={{2,.8}};v.memory.people={p};auto x=evaluate_social(v,Interaction::RomanticTouch,p.id,0);v.memory.people[0].known_kin=true;auto y=evaluate_social(v,Interaction::RomanticTouch,p.id,0);CHECK(y.moral>=x.moral);CHECK(std::isfinite(y.score));CHECK(y.known);}
TEST("community",stranger_romance_is_cost_not_physical_gate){auto w=World::generate(42,8);w.configure_community();auto v=w.personal_view(1).social;v.memory.methods[1].mastery=.9;PersonBelief p;p.id=2;p.known_kin=false;p.gender=Gender::Unknown;v.familiarities={{2,0}};v.memory.people={p};auto x=evaluate_social(v,Interaction::RomanticTouch,p.id,0);v.familiarities[0].second=1;auto y=evaluate_social(v,Interaction::RomanticTouch,p.id,0);CHECK(x.moral>y.moral);CHECK(x.known);}
TEST("community",food_price_transfers_exactly_once){auto w=World::generate(42,8);w.configure_community();auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=3;auto cash=s.actors[0].money;w.command_for_test(1,Method::AcquireFood,3);w.run_seconds(60);NEAR(s.actors[0].money,cash-12,1e-10);NEAR(s.actors[0].economy.food_spent,12,1e-10);w.validate();}
TEST("community",paid_leisure_is_charged_on_entry_once){auto w=World::generate(42,8);w.configure_community();auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=10;auto cash=s.actors[0].money;w.command_for_test(1,Method::Leisure,10);NEAR(s.actors[0].money,cash-5,1e-10);auto p=(std::filesystem::temp_directory_path()/"community-fee.save").string();w.save(p);auto r=World::load(p);r.run_seconds(1800);NEAR(r.state().actors[0].money,cash-5,1e-10);r.validate();std::filesystem::remove(p);}
TEST("community",qualification_not_unlocked_by_gossip_count){auto w=World::generate(42,8);w.configure_community();at_work_time(w);auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=9;auto cash=s.actors[0].money;for(unsigned i=0;i<100;++i)s.actors[0].mind.social.community.receive(example_news(10000+i),2,20000+i,s.now);w.command_for_test(1,Method::Work,9);w.run_seconds(1800);NEAR(s.actors[0].money,cash,1e-10);CHECK(s.actors[0].completed[std::size_t(Method::Work)]==0);}
TEST("community",study_teaches_content_not_gossip){auto w=World::generate(42,8);w.configure_community();auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=6;auto before=s.actors[0].mind.knowledge.get(702);w.command_for_test(1,Method::Study,6);w.run_seconds(1800);CHECK(s.actors[0].mind.knowledge.get(702)>before);CHECK(s.actors[0].economy.study_sessions==1);NEAR(s.actors[0].economy.study_spent,4,1e-10);}
TEST("community",qualified_job_pays_higher_wage){auto w=World::generate(42,8);w.configure_community();at_work_time(w);auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=7;for(auto& [id,g]:s.actors[0].mind.knowledge.mastery)if(id==702)g=.9;auto cash=s.actors[0].money;w.command_for_test(1,Method::Work,7);w.run_seconds(1800);NEAR(s.actors[0].money,cash+18.75/2,1e-8);CHECK(s.actors[0].economy.job==1);CHECK(s.actors[0].economy.promotions==1);w.validate();}
TEST("community",interrupted_work_keeps_earned_credit){auto w=World::generate(42,8);w.configure_community();at_work_time(w);auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=5;auto cash=s.actors[0].money;w.command_for_test(1,Method::Work,5);w.run_seconds(900);w.command_for_test(1,Method::Idle,5);NEAR(s.actors[0].money,cash+12.5/4,1e-8);w.validate();}
TEST("community",accounting_is_exclusive){auto w=World::generate(42,8);w.configure_community();w.run_seconds(3600);for(auto& a:w.state().actors){double t=0;for(double x:a.economy.time_ms)t+=x;NEAR(t,double(a.physical_at),1e-5);}}
TEST("community",unknown_balance_not_in_other_opinion){auto a=information_room();put_news(a);auto b=a;auto& s=b.edit_for_test();s.actors[7].money+=1000;s.ledger.initial_money+=1000;auto x=a.personal_view(1),y=b.personal_view(1);CHECK(x.social.memory.community.entries.size()==y.social.memory.community.entries.size());a.run_seconds(120);b.run_seconds(120);CHECK(a.state().actors[0].cog.counts==b.state().actors[0].cog.counts);}
TEST("community",conversation_content_not_hidden_by_talk_destinations){auto w=information_room();put_news(w);auto v=w.personal_view(1);v.need[5]=.9;auto ideas=Planner::ideas(v,w.state().random);CHECK(std::any_of(ideas.begin(),ideas.end(),[](auto& x){return x.method==Method::Social;}));CHECK(std::none_of(ideas.begin(),ideas.end(),[](auto& x){return x.method==Method::Talk;}));}
TEST("community_unit",memory_payload_invalid_rejected){CommunityMemory m;auto f=example_news();f.confidence=std::nan("");THROWS(m.receive(f,1,10,0));}
TEST("community",learning_preserves_known_weekend_living_reserve){auto w=World::generate(42,8);w.configure_community();auto& s=w.edit_for_test();s.now=4*86400000LL+18*3600000LL;s.next_physical=s.now;for(auto& a:s.actors)a.physical_at=s.now;auto v=w.personal_view(1);CHECK(v.economy.study_budget==0);}
TEST("community",thirst_focus_retrieves_known_water_before_leisure_variants){auto w=World::generate(42,8);w.configure_community();auto v=w.personal_view(1);v.need.fill(0);v.need[1]=.8;v.need[4]=.95;v.focus_metric=1;auto ideas=Planner::ideas(v,w.state().random);CHECK(!ideas.empty());CHECK(ideas.front().method==Method::Drink);}
TEST("community_unit",study_thought_has_russian_localization){Thought t;t.kind=ThoughtKind::Recall;t.method=Method::Study;CHECK(!thought_text(t).empty());}
TEST("community",qualified_opportunity_survives_bounded_retrieval){auto w=World::generate(42,8);w.configure_community();at_work_time(w);auto v=w.personal_view(1);v.focus_metric=7;v.economy.mastery[1]=.8;v.need.fill(0);v.need[7]=.9;auto x=Planner::ideas(v,w.state().random);CHECK(!x.empty());CHECK(x[0].method==Method::Work);CHECK(x[0].place==7);}
TEST("community_unit",many_news_do_not_erase_known_discussion){SocialView v;v.enabled=v.in_conversation=true;v.partner=2;v.self=1;v.memory.community.enabled=true;v.memory.methods[std::size_t(Interaction::ShareNews)]={.9,.4,.9,.9,1};v.memory.methods[std::size_t(Interaction::DiscussTopic)]={.9,.4,.9,.9,2};for(unsigned i=1;i<30;++i)v.memory.community.receive(example_news(i),0,100+i,0);auto choices=social_choices(v);CHECK(std::any_of(choices.begin(),choices.end(),[](auto& x){return x.kind==Interaction::DiscussTopic;}));}
TEST("community",many_claims_do_not_hide_other_social_ways_in_planner){auto w=information_room();auto& a=w.edit_for_test().actors[0];a.mind.social.methods[std::size_t(Interaction::DiscussTopic)]={.9,.4,.9,.9,999};for(int i=0;i<20;++i)put_news(w);auto v=w.personal_view(1);v.focus_metric=5;auto xs=Planner::ideas(v,w.state().random);CHECK(xs.size()>1);CHECK(xs[1].interaction==Interaction::DiscussTopic);}
TEST("community_unit",appearance_preference_invalid_rejected){Appearance a;Preference p;p.height_weight=.9;p.build_weight=.8;THROWS(attraction(a,p));}
TEST("community",failed_job_attempt_does_not_count_as_work){auto w=World::generate(42,8);w.configure_community();at_work_time(w);auto& s=w.edit_for_test();s.autonomy=false;s.actors[0].place=9;w.command_for_test(1,Method::Work,9);w.run_seconds(60);NEAR(s.actors[0].economy.work_today,0,0);}
TEST("community",spending_policy_reserves_weekend_food_and_rent){auto w=World::generate(42,8);w.configure_community();auto& s=w.edit_for_test();s.now=4*86400000LL+18*3600000LL;s.next_physical=s.now;for(auto& a:s.actors)a.physical_at=s.now;auto v=w.personal_view(1);v.need[4]=.95;for(auto& i:Planner::ideas(v,s.random))if(i.method==Method::Leisure){auto p=std::find_if(v.places.begin(),v.places.end(),[&](auto& p){return p.id==i.place;});CHECK(p->price==0);}}
TEST("community_unit",remembered_dishonesty_changes_disclosure_risk){SocialView v;v.enabled=true;v.self=1;v.partner=2;v.memory.community.enabled=true;v.memory.community.confidentiality=0;v.memory.methods[std::size_t(Interaction::ShareNews)]={1,.5,1,1,1};auto f=example_news(10);f.disclosure=Disclosure::Entrusted;f.sensitivity=1;v.memory.community.receive(f,0,100,0);auto o=example_news(11);o.kind=NewsKind::Opinion;o.subject=2;o.valence=1;v.memory.community.receive(o,3,101,0);auto good=evaluate_social(v,Interaction::ShareNews,2,10);v.memory.community.entries.back().content.valence=-1;auto bad=evaluate_social(v,Interaction::ShareNews,2,10);CHECK(bad.moral>good.moral);CHECK(bad.score<good.score);}
TEST("community",bad_serialized_economy_rejected){auto w=World::generate(42,8);w.configure_community();w.edit_for_test().actors[0].economy.earned=std::nan("");THROWS(w.validate());}
TEST("community",bad_serialized_memory_rejected){auto w=World::generate(42,8);w.configure_community();w.edit_for_test().actors[0].mind.social.community.entries[0].gist.half_life_hours=-1;THROWS(w.validate());}
TEST("community",failed_focused_plan_does_not_starve_other_urgent_topics){
 auto w=World::generate(7,16);w.configure_community();auto& s=w.edit_for_test();
 auto& a=s.actors[11];a.place=4;a.body.water=0;a.body.energy=0;a.body.sleep=1;
 a.body.damage=.14;a.body.fatigue=0;a.social=a.leisure=0;a.desire.value=0;a.mind.social.enabled=false;
 a.mind.cognition.base={.715227,.408499,.540756,.677219,.352984,.367636,.552865,.477075};
 a.mind.known.fill(false);a.mind.known[std::size_t(Method::Sleep)]=true;a.mind.known[std::size_t(Method::Drink)]=true;
 auto& q=a.mind.learning.q[std::size_t(Method::Sleep)][std::size_t(Metric::Sleep)];q.mean=.000970417;q.count=53;q.variance=.001;
 for(unsigned i=0;i<3;++i){auto& n=a.cog.needs[i];n.status=Truth::Confirmed;n.mean=i==2?1:.9;n.observation=n.prior=n.mean;n.quality=.9;n.prior_quality=.5;n.source=100+i;n.prior_source=200+i;}
 a.cog.focus=3;a.cog.focus_metric=2;a.cog.focus_kind=TopicKind::Internal;
 w.run_seconds(180);
 CHECK(s.actors[11].completed[std::size_t(Method::Drink)]>0);
}
TEST("community_long",seed7_available_water_does_not_lose_to_failed_sleep_forecast){
 auto w=World::generate(7,16);w.configure_community();w.run_seconds(3*86400);
 const auto& a=w.state().actors[11];std::cerr<<"probe water="<<a.body.water<<" place="<<a.place<<" money="<<a.money<<"\n";
 CHECK(a.body.water>.1);
}
