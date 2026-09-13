#include "test.hpp"
#if __has_include("life/self_model.hpp")
#include "life/self_model.hpp"
#include "life/archive.hpp"
#include <limits>
using namespace life;
namespace {
InterpretedEpisode event(std::uint64_t id, double success=1, double own=.9) {
    InterpretedEpisode e; e.id=e.source_event=id; e.source_action=id;
    e.actual=e.intentional=true; e.domain=SelfDomain::Social;
    e.success=success; e.importance=e.perception_quality=1;
    e.attribution={own,0,0,1-own,.9,1}; e.time=Tick(id)*1000;
    return e;
}
std::string bytes(const SelfModel& m){ Writer w; w(m); return w.data; }
}
TEST("self_model",neutral_without_experience) {
    SelfModel m; auto p=m.predict(SelfDomain::Social);
    NEAR(p.efficacy,.5,0); NEAR(p.control,.5,0); NEAR(p.coping,.5,0);
    NEAR(p.acceptance,.5,0); NEAR(p.uncertainty_tolerance,.5,0); CHECK(p.confidence==0);
}
TEST("self_model",self_attributed_success_increases_efficacy) {
    SelfModel m; Cognitive c; CHECK(m.integrate(event(1),c,.5));
    CHECK(m.predict(SelfDomain::Social).efficacy>.5);
}
TEST("self_model",luck_attributed_success_barely_changes_efficacy) {
    SelfModel own,luck; Cognitive c;
    for(unsigned i=1;i<=20;++i){own.integrate(event(i,1,.95),c,.5);luck.integrate(event(i,1,.01),c,.5);}
    CHECK(own.predict(SelfDomain::Social).efficacy-luck.predict(SelfDomain::Social).efficacy>.1);
}
TEST("self_model",external_failure_does_not_destroy_efficacy) {
    SelfModel m; auto e=event(1,0,0);e.attribution={0,0,1,0,.2,1};
    m.integrate(e,Cognitive{},.5); NEAR(m.schemas[1].beliefs[0].value(),.5,1e-12);
}
TEST("self_model",failure_without_recovery_does_not_fake_coping) {
    SelfModel m;auto e=event(1,0);e.adversity=.9;e.uncertainty=.8;
    m.integrate(e,Cognitive{},.5);NEAR(m.predict(SelfDomain::Social).coping,.5,1e-12);
    NEAR(m.predict(SelfDomain::Social).uncertainty_tolerance,.5,1e-12);
}
TEST("self_model",failure_can_increase_coping_after_observed_recovery) {
    SelfModel m;auto e=event(1,0);e.adversity=.9;e.uncertainty=.8;
    m.integrate(e,Cognitive{},.5);e.stage=SelfEpisodeStage::Recovery;
    e.recovery_observed=true;e.recovered=.9;e.functioning=.9;e.consequences=.1;
    e.elapsed_recovery_ms=1800000;e.time+=1800000;CHECK(m.integrate(e,Cognitive{},.5));
    CHECK(m.predict(SelfDomain::Social).efficacy<.5);
    CHECK(m.predict(SelfDomain::Social).coping>.5);
    CHECK(m.predict(SelfDomain::Social).uncertainty_tolerance>.5);
}
TEST("self_model",early_or_unobserved_recovery_is_rejected) {
    SelfModel m;auto e=event(1,0);m.integrate(e,Cognitive{},.5);
    auto before=bytes(m);e.stage=SelfEpisodeStage::Recovery;e.recovered=1;e.functioning=1;
    CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
    e.recovery_observed=true;e.elapsed_recovery_ms=0;CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
}
TEST("self_model",forecast_and_unperceived_episode_never_update_self) {
    SelfModel m;auto before=bytes(m);auto e=event(1);e.actual=false;
    CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
    e.actual=true;e.perception_quality=0;CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
}
TEST("self_model",duplicate_source_not_reinforced_even_with_new_episode_id) {
    SelfModel m;auto e=event(1);CHECK(m.integrate(e,Cognitive{},.5));auto before=bytes(m);
    e.id=999;CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
}
TEST("self_model",retired_source_cannot_reinforce_after_hot_cache_rotates) {
    SelfModel m;for(unsigned i=1;i<=400;++i)m.integrate(event(i),Cognitive{},.5);
    auto before=bytes(m);CHECK(!m.integrate(event(1),Cognitive{},.5));CHECK(bytes(m)==before);
    CHECK(m.primary_sources.size()<=128);
}
TEST("self_model",domain_specificity_and_weak_generalization) {
    SelfModel m;for(unsigned i=1;i<=10;++i){auto e=event(i);e.domain=SelfDomain::Work;m.integrate(e,Cognitive{},.5);}
    const auto work=m.predict(SelfDomain::Work).efficacy-.5;
    const auto general=m.predict(SelfDomain::General).efficacy-.5;
    CHECK(work>0);CHECK(general>0);CHECK(general<.30*work);
    CHECK(m.predict(SelfDomain::Romance).efficacy-.5<.1*work);
}
TEST("self_model",acceptance_is_separate_from_success) {
    SelfModel m;auto e=event(1);m.integrate(e,Cognitive{},.5);
    NEAR(m.predict(SelfDomain::Social).acceptance,.5,1e-12);
    e=event(2,0);e.acceptance_observed=true;e.accepted=0;m.integrate(e,Cognitive{},.5);
    CHECK(m.predict(SelfDomain::Social).acceptance<.5);
}
TEST("self_model",known_control_can_survive_failure) {
    SelfModel m;auto e=event(1,0);e.attribution.controllability=.9;
    m.integrate(e,Cognitive{},.5);CHECK(m.predict(SelfDomain::Social).efficacy<.5);CHECK(m.predict(SelfDomain::Social).control>.5);
}
TEST("self_model",tolerance_requires_remembered_uncertainty) {
    SelfModel m;auto e=event(1);e.uncertainty=0;m.integrate(e,Cognitive{},.5);
    NEAR(m.predict(SelfDomain::Social).uncertainty_tolerance,.5,1e-12);
}
TEST("self_model",same_failure_different_recovery) {
    SelfModel a,b;auto e=event(1,0);e.adversity=.9;
    a.integrate(e,Cognitive{},.5);b.integrate(e,Cognitive{},.5);
    e.stage=SelfEpisodeStage::Recovery;e.recovery_observed=true;e.elapsed_recovery_ms=1800000;e.time+=1800000;
    e.recovered=e.functioning=1;e.consequences=0;a.integrate(e,Cognitive{},.5);
    e.recovered=e.functioning=.1;e.consequences=.8;b.integrate(e,Cognitive{},.5);
    CHECK(a.predict(SelfDomain::Social).coping>b.predict(SelfDomain::Social).coping);
}
TEST("self_model",invalid_inputs_fail_before_mutating_any_belief) {
    SelfModel m;auto before=bytes(m);auto e=event(1);e.attribution.self=2;
    THROWS(m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
    e=event(1);e.success=std::numeric_limits<double>::quiet_NaN();THROWS(m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
    e=event(1);e.domain=SelfDomain::Count;THROWS(m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
    e=event(1);e.source_event=0;CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
    THROWS(m.predict(SelfDomain::Count));
}
TEST("self_model",self_model_roundtrip_and_bounded_provenance) {
    SelfModel m;for(unsigned i=1;i<=40;++i)m.integrate(event(i),Cognitive{},.5);
    Writer w;w(m);Reader r(w.data);SelfModel other;r(other);CHECK(r.finished());CHECK(bytes(other)==bytes(m));other.validate();
    for(const auto& s:m.schemas)for(const auto& b:s.beliefs)CHECK(b.root_count<=8);
}
TEST("self_model",diagnostics_do_not_change_learning) {
    SelfModel a,b;std::vector<SelfUpdateTrace> log;
    auto e=event(1);a.integrate(e,Cognitive{},.5,[&](const auto& t){log.push_back(t);});b.integrate(e,Cognitive{},.5);
    CHECK(!log.empty());CHECK(bytes(a)==bytes(b));CHECK(log.front().source==1);
}
TEST("self_model",old_unseen_source_rejection_is_transactional) {
    SelfModel m;for(unsigned i=1000;i<1128;++i)m.integrate(event(i),Cognitive{},.5);
    auto before=bytes(m);CHECK(!m.integrate(event(1),Cognitive{},.5));
    CHECK(bytes(m)==before);m.validate();
}
TEST("self_model",calm_without_functioning_does_not_fake_recovery) {
    SelfModel m;auto e=event(1,0);e.adversity=.9;m.integrate(e,Cognitive{},.5);
    e.stage=SelfEpisodeStage::Recovery;e.recovery_observed=true;e.elapsed_recovery_ms=1800000;
    e.recovered=1;e.functioning=0;e.consequences=0;e.time+=1800000;m.integrate(e,Cognitive{},.5);
    CHECK(m.predict(SelfDomain::Social).coping<=.5);
}
TEST("self_model",unseen_retired_root_cannot_invent_recovery) {
    SelfModel m;for(unsigned i=1000;i<1300;++i)m.integrate(event(i),Cognitive{},.5);
    auto e=event(1,0);e.stage=SelfEpisodeStage::Recovery;e.recovery_observed=true;e.elapsed_recovery_ms=1800000;e.time=2000000;e.functioning=e.recovered=1;
    auto before=bytes(m);CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
}
TEST("self_model",recovery_cannot_change_unrelated_domain) {
    SelfModel m;auto e=event(1,0);e.adversity=.9;m.integrate(e,Cognitive{},.5);
    e.stage=SelfEpisodeStage::Recovery;e.recovery_observed=true;e.elapsed_recovery_ms=1800000;e.time+=1800000;e.functioning=e.recovered=1;e.domain=SelfDomain::Romance;
    auto before=bytes(m);CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
}
TEST("self_model",recovery_requires_elapsed_real_timestamp) {
    SelfModel m;auto e=event(1,0);e.adversity=.9;m.integrate(e,Cognitive{},.5);
    e.stage=SelfEpisodeStage::Recovery;e.recovery_observed=true;e.elapsed_recovery_ms=1800000;e.functioning=e.recovered=1;
    auto before=bytes(m);CHECK(!m.integrate(e,Cognitive{},.5));CHECK(bytes(m)==before);
}

#else
TEST("self_model",self_model_module_must_exist){CHECK(false && "Missing SELF-MODEL-0.1 implementation");}
#endif
