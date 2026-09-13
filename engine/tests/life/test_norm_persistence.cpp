#include "test.hpp"

#include "life/archive.hpp"
#include "life/world.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace life;

namespace {

NormKey persistence_key(std::uint32_t variant=1) {
    return {practice_id(NormPractice::Help),61,62,0,variant};
}

NormObservation persistence_observation(std::uint64_t delivery,NormKey key_value=persistence_key()) {
    NormObservation observation;
    observation.key=key_value;
    observation.source={delivery,delivery+50'000,1,2,NormOrigin::Observed,true};
    observation.at=0;
    observation.observed_actor=2;
    observation.quality=1;
    observation.confidence=1;
    observation.reliability=1;
    observation.dose=1;
    observation.value=.75;
    observation.channel=NormChannel::Descriptive;
    observation.applicable=true;
    observation.value_known=true;
    observation.outcome_window_complete=true;
    return observation;
}

World persistent_world() {
    auto world=World::generate(42,8);
    NormProfile profile;
    profile.mode=NormMode::Enabled;
    world.configure_norm_memory(profile);
    return world;
}

NormSource persistence_prior(std::uint64_t id) {
    return {id,id,1,0,NormOrigin::LegacyPrior,true};
}

template<class Value>
std::string persistence_bytes(const Value& value) {
    Writer writer;
    writer(value);
    return writer.data;
}

template<class Predicate>
bool run_until(World& world,Predicate predicate,unsigned iterations=12'000) {
    for(unsigned i=0;i<iterations;++i) {
        if(predicate()) return true;
        world.run_ms(25);
    }
    return predicate();
}

std::filesystem::path clean_temp(const char* filename) {
    auto path=std::filesystem::temp_directory_path()/filename;
    std::error_code ignored;
    std::filesystem::remove(path,ignored);
    std::filesystem::remove(path.string()+".tmp",ignored);
    return path;
}

std::string norm_trace_signature(const NormTrace& trace) {
    return norm_trace_json(trace);
}

} // namespace

TEST("norm_persistence",nm079_restore_between_interpret_and_integrate_is_exactly_once) {
    auto world=persistent_world();
    const auto key=persistence_key(79);
    world.norm_observation_for_test(1,persistence_observation(900'079,key));
    CHECK(run_until(world,[&]{
        return world.state().actors[0].cog.operation==Operation::IntegrateNormEvidence;
    }));
    CHECK(world.state().actors[0].mind.norm_memory.record_revision(key)==0);

    const auto path=clean_temp("life-norm-nm079.save");
    world.save(path.string());
    auto restored=World::load(path.string());
    CHECK(restored.hash()==world.hash());

    CHECK(run_until(world,[&]{
        return world.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    CHECK(run_until(restored,[&]{
        return restored.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    world.run_seconds(60);
    restored.run_seconds(60);
    CHECK(restored.hash()==world.hash());
    CHECK(world.state().actors[0].cog.norm.integrated==1);
    const auto& records=world.state().actors[0].mind.norm_memory.records();
    const auto record=std::find_if(records.begin(),records.end(),
        [&](const auto& value){return value.key==key;});
    CHECK(record!=records.end());
    CHECK(record->sources.size()==1);
    std::filesystem::remove(path);
}

TEST("norm_persistence",nm081_hot_and_cold_records_survive_restore_and_future_paid_work) {
    auto world=persistent_world();
    auto& memory=world.edit_for_test().actors[0].mind.norm_memory;
    for(std::uint32_t i=0;i<129;++i)
        memory.seed_binary(persistence_key(1'000+i),NormChannel::Descriptive,
                           double(i%5)/4,2,persistence_prior(810'000+i),0);
    CHECK(memory.size()>=129);
    const auto cold=persistence_key(1'128);
    CHECK(memory.predict(cold,0).descriptive_known);

    const auto path=clean_temp("life-norm-nm081.save");
    world.save(path.string());
    auto restored=World::load(path.string());
    CHECK(restored.hash()==world.hash());
    CHECK(restored.state().actors[0].mind.norm_memory.size()==memory.size());
    NEAR(restored.state().actors[0].mind.norm_memory.predict(cold,0).prevalence,
         memory.predict(cold,0).prevalence,1e-12);

    const auto future=persistence_key(2'000);
    world.norm_observation_for_test(1,persistence_observation(900'081,future));
    restored.norm_observation_for_test(1,persistence_observation(900'081,future));
    world.run_seconds(120);
    restored.run_seconds(120);
    CHECK(restored.hash()==world.hash());
    CHECK(world.state().actors[0].mind.norm_memory.record_revision(future)!=0);
    std::filesystem::remove(path);
}

TEST("norm_persistence",nm082_nm083_workers_index_and_logs_do_not_change_state) {
    auto one=persistent_world();
    auto two=one;
    auto four=one;
    one.set_workers(1,4);
    two.set_workers(2,2);
    two.set_indexed(false);
    four.set_workers(4,3);
    std::vector<std::string> one_norm,two_norm,four_norm;
    std::vector<std::string> one_thoughts,two_thoughts,four_thoughts;
    std::uint64_t norm_traces=0;
    std::uint64_t event_traces=0;
    one.set_norm_logger([&](const NormTrace& trace){one_norm.push_back(norm_trace_signature(trace));});
    two.set_norm_logger([&](const NormTrace& trace){two_norm.push_back(norm_trace_signature(trace));});
    four.set_norm_logger([&](const NormTrace& trace){++norm_traces;four_norm.push_back(norm_trace_signature(trace));});
    one.set_thought_logger([&](const Thought& thought){one_thoughts.push_back(thought_json(thought));});
    two.set_thought_logger([&](const Thought& thought){two_thoughts.push_back(thought_json(thought));});
    four.set_thought_logger([&](const Thought& thought){four_thoughts.push_back(thought_json(thought));});
    four.set_logger([&](const EventLog&){++event_traces;});
    const auto observation=persistence_observation(900'082,persistence_key(82));
    one.norm_observation_for_test(1,observation);
    two.norm_observation_for_test(1,observation);
    four.norm_observation_for_test(1,observation);
    one.run_seconds(180);
    two.run_seconds(180);
    four.run_seconds(180);
    CHECK(one.hash()==two.hash());
    CHECK(two.hash()==four.hash());
    CHECK(one_norm==two_norm);
    CHECK(two_norm==four_norm);
    CHECK(one_thoughts==two_thoughts);
    CHECK(two_thoughts==four_thoughts);
    CHECK(norm_traces>0);
    CHECK(event_traces>0);
}

TEST("norm_persistence",nm082_context_compare_stage_restore_is_worker_invariant) {
    auto world=persistent_world();
    auto& actor=world.edit_for_test().actors[0];
    const NormKey key{practice_id(NormPractice::Work),0,0,0,82};
    actor.mind.norm_memory.seed_approval(key,{.1,.8,.1},8,persistence_prior(820'001),0);
    world.set_workers(4,3);
    world.norm_context_for_test(1);
    CHECK(run_until(world,[&]{
        const auto& cognition=world.state().actors[0].cog;
        return cognition.operation==Operation::CompareNormAlternatives&&
               cognition.due>world.state().now;
    }));

    const auto path=clean_temp("life-norm-nm082-context-stage.save");
    world.save(path.string());
    auto restored=World::load(path.string());
    restored.set_workers(2,2);
    CHECK(restored.hash()==world.hash());
    std::vector<std::string> original_trace,restored_trace;
    world.set_norm_logger([&](const NormTrace& trace){original_trace.push_back(norm_trace_signature(trace));});
    restored.set_norm_logger([&](const NormTrace& trace){restored_trace.push_back(norm_trace_signature(trace));});
    world.run_seconds(180);
    restored.run_seconds(180);
    CHECK(restored.hash()==world.hash());
    CHECK(restored_trace==original_trace);
    CHECK(world.state().actors[0].cog.norm.deep_evaluations>0);
    std::filesystem::remove(path);
}

TEST("norm_persistence",nm085_old_save_version_is_rejected_explicitly) {
    const auto path=clean_temp("life-norm-nm085.save");
    {
        std::ofstream output(path,std::ios::binary|std::ios::trunc);
        output<<"LIFE-SAVE-0.13.0\nold-payload";
    }
    THROWS(World::load(path.string()));
    std::filesystem::remove(path);
}

TEST("norm_persistence",prior_norm_schema_is_rejected_after_provenance_append) {
    const auto path=clean_temp("life-norm-r1-schema.save");
    {
        std::ofstream output(path,std::ios::binary|std::ios::trunc);
        output<<"LIFE-SAVE-0.14.0-norm01-r1\nold-payload";
    }
    THROWS(World::load(path.string()));
    std::filesystem::remove(path);
}

TEST("norm_persistence",nm087_no_new_basis_cannot_repeat_integration) {
    auto world=persistent_world();
    const auto key=persistence_key(87);
    world.norm_observation_for_test(1,persistence_observation(900'087,key));
    CHECK(run_until(world,[&]{
        return world.state().actors[0].mind.norm_memory.record_revision(key)!=0;
    }));
    auto& state=world.edit_for_test();
    state.autonomy=false;
    const auto target_revision=state.actors[0].mind.norm_memory.record_revision(key);
    const auto integrated=state.actors[0].cog.norm.integrated;
    const auto now=state.now;
    world.run_seconds(300);
    const auto& actor=world.state().actors[0];
    CHECK(world.state().now==now+300'000);
    CHECK(actor.mind.norm_memory.record_revision(key)==target_revision);
    CHECK(actor.cog.norm.integrated==integrated);
    CHECK(actor.cog.norm.inbox.empty());
}

TEST("norm_persistence",active_cognition_roundtrips_moral_provenance_components) {
    auto world=persistent_world();
    world.norm_context_for_test(1);
    auto& actor=world.edit_for_test().actors[0];
    CHECK(actor.cog.active);
    CHECK(actor.cog.operation==Operation::RecallNormContext);

    const NormKey privacy{practice_id(NormPractice::Privacy),0,0,0,1};
    auto& decision=actor.cog.current;
    decision.norm_ledger.clear();
    ConsequenceTerm principle;
    principle.key={ConsequenceKind::PersonalPrinciple,actor.id,
                   practice_id(NormPractice::Privacy),0};
    principle.owner=ForecastOwner::NormPrior;
    principle.amount=-.55;
    principle.present_value=true;
    principle.source_revision=91;
    principle.norm_key=privacy;
    decision.norm_ledger.push_back(principle);
    decision.norm_raw=-.55;

    auto& social=decision.social_evaluation;
    social.normative_effect_count=1;
    social.normative_effects[0].key=privacy;
    social.normative_effects[0].source_revision=91;
    social.normative_effects[0].origin=NormOrigin::Historical;
    social.normative_effects[0].moral_cost=.55;
    social.practical_moral=.12;
    social.practical_moral_source_revision=92;
    social.assumed_normative_moral=.08;
    social.assumed_normative_source_version=1;
    const auto current_bytes=persistence_bytes(decision);

    const auto path=clean_temp("life-norm-moral-provenance-active.save");
    world.save(path.string());
    auto restored=World::load(path.string());
    CHECK(restored.hash()==world.hash());
    const auto& restored_actor=restored.state().actors[0];
    CHECK(restored_actor.cog.active);
    CHECK(restored_actor.cog.operation==Operation::RecallNormContext);
    CHECK(persistence_bytes(restored_actor.cog.current)==current_bytes);
    const auto& restored_decision=restored_actor.cog.current;
    CHECK(restored_decision.norm_ledger.size()==1);
    CHECK(restored_decision.norm_ledger[0].norm_key==privacy);
    CHECK(restored_decision.norm_ledger[0].source_revision==91);
    CHECK(restored_decision.social_evaluation.normative_effect_count==1);
    CHECK(restored_decision.social_evaluation.normative_effects[0].key==privacy);
    CHECK(restored_decision.social_evaluation.normative_effects[0].source_revision==91);
    NEAR(restored_decision.social_evaluation.practical_moral,.12,1e-15);
    CHECK(restored_decision.social_evaluation.practical_moral_source_revision==92);
    NEAR(restored_decision.social_evaluation.assumed_normative_moral,.08,1e-15);
    CHECK(restored_decision.social_evaluation.assumed_normative_source_version==1);

    world.run_seconds(60);
    restored.run_seconds(60);
    CHECK(restored.hash()==world.hash());
    std::filesystem::remove(path);
}

TEST("norm_persistence",malformed_active_moral_component_count_is_rejected) {
    auto world=persistent_world();
    world.norm_context_for_test(1);
    auto& cognition=world.edit_for_test().actors[0].cog;
    CHECK(cognition.active);
    cognition.current.social_evaluation.normative_effect_count=5;
    THROWS(world.validate());
}

TEST("norm_persistence",central_norm_trace_json_keeps_full_key_and_escapes_text) {
    NormTrace trace;
    trace.at=77;
    trace.actor=3;
    trace.kind="ledger\"row";
    trace.reason="privacy\\line\n";
    trace.key={practice_id(NormPractice::Privacy),61,62,7,1};
    trace.source_revision=91;
    trace.ledger_owner=std::uint8_t(ForecastOwner::NormPrior);
    trace.consequence_kind=std::uint8_t(ConsequenceKind::PersonalPrinciple);
    trace.knownness=std::uint8_t(ConsequenceKnownness::Known);
    trace.candidate_method=std::uint32_t(Method::Social);
    trace.candidate_interaction=std::uint32_t(Interaction::ShareNews);
    trace.candidate_object=123;
    const auto json=norm_trace_json(trace);
    CHECK(json.find("\"kind\":\"ledger\\\"row\"")!=std::string::npos);
    CHECK(json.find("\"reason\":\"privacy\\\\line\\u000a\"")!=std::string::npos);
    CHECK(json.find("\"practice\":5")!=std::string::npos);
    CHECK(json.find("\"group\":61")!=std::string::npos);
    CHECK(json.find("\"context\":62")!=std::string::npos);
    CHECK(json.find("\"actor_role\":7")!=std::string::npos);
    CHECK(json.find("\"variant\":1")!=std::string::npos);
    CHECK(json.find("\"source_revision\":91")!=std::string::npos);
    CHECK(json.find("\"candidate_interaction\":9")!=std::string::npos);
    CHECK(json.find("\n")==std::string::npos);
}
