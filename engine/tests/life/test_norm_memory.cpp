#include "test.hpp"

#include "life/archive.hpp"
#include "life/norm_memory.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

using namespace life;

namespace {

constexpr Tick game_day_ms=86'400'000;

NormKey key(std::uint32_t practice=1,std::uint32_t variant=1) {
    return {practice,7,3,2,variant};
}

NormSource source(std::uint64_t delivery=1,std::uint64_t root=1,
                  std::uint64_t revision=1,NormOrigin origin=NormOrigin::Observed) {
    return {delivery,root,revision,4,origin,true};
}

NormObservation binary_observation(std::uint64_t delivery,std::uint64_t root,
                                   double value=1,double dose=1,Tick at=0) {
    NormObservation result;
    result.key=key();
    result.source=source(delivery,root);
    result.at=at;
    result.observed_actor=20+Id(root);
    result.quality=1;
    result.confidence=1;
    result.reliability=1;
    result.dose=dose;
    result.value=value;
    result.channel=NormChannel::Descriptive;
    result.applicable=true;
    result.value_known=true;
    result.outcome_window_complete=true;
    return result;
}

NormObservation approval_observation(std::uint64_t delivery,std::uint64_t root,
                                     ApprovalValue value,Tick at=0) {
    auto result=binary_observation(delivery,root,1,1,at);
    result.channel=NormChannel::Approval;
    result.approval=value;
    return result;
}

NormObservation principle_argument(std::uint64_t delivery,std::uint64_t root,
                                   double target,Tick at=0) {
    auto result=binary_observation(delivery,root,target,1,at);
    result.channel=NormChannel::PersonalPrinciple;
    result.accepted_argument=true;
    result.plasticity=.5;
    result.principle_aspect=91;
    result.principle_applicability=1;
    result.principle_exception=0;
    result.principle_severity=1;
    return result;
}

std::string encoded(const NormMemory& memory) {
    Writer writer;
    writer(memory);
    return writer.data;
}

} // namespace

TEST("norm_memory", nm001_no_evidence_is_unknown_not_social_permission) {
    BinaryNormEstimate estimate;
    CHECK(!estimate.known());
    NEAR(estimate.probability(),.5,1e-12);
    NEAR(estimate.coverage(),0,1e-12);
    const auto prediction=NormMemory{}.predict(key(),0);
    CHECK(!prediction.descriptive_known);
    CHECK(!prediction.approval_known);
    CHECK(!prediction.personal_principle_known);
    NEAR(prediction.prevalence,.5,1e-12);
}

TEST("norm_memory", nm002_descriptive_evidence_does_not_change_approval_or_principle) {
    NormMemory memory;
    CHECK(memory.apply(binary_observation(1,1),0)==ApplyEvidence::Added);
    const auto prediction=memory.predict(key(),0);
    CHECK(prediction.descriptive_known);
    CHECK(!prediction.approval_known);
    CHECK(!prediction.personal_principle_known);
    NEAR(prediction.personal_resistance,0,1e-12);
}

TEST("norm_memory", nm003_approval_keeps_indifference_as_a_third_outcome) {
    ApprovalEstimate estimate;
    estimate.evidence={5,2,1};
    const auto p=estimate.probabilities();
    NEAR(p[0],6./11,1e-12);
    NEAR(p[1],3./11,1e-12);
    NEAR(p[2],2./11,1e-12);
    CHECK(std::abs(p[1]-(1-p[0]))>.1);
    NormMemory memory;
    std::uint64_t delivery=1;
    for(int i=0;i<5;++i,++delivery) memory.apply(approval_observation(delivery,delivery,ApprovalValue::Approve),0);
    for(int i=0;i<2;++i,++delivery) memory.apply(approval_observation(delivery,delivery,ApprovalValue::Disapprove),0);
    memory.apply(approval_observation(delivery,delivery,ApprovalValue::Indifferent),0);
    const auto prediction=memory.predict(key(),0);
    NEAR(prediction.approve,6./11,1e-12);
    NEAR(prediction.disapprove,3./11,1e-12);
    NEAR(prediction.indifferent,2./11,1e-12);
}

TEST("norm_memory", nm004_invalid_input_throws_before_any_mutation) {
    NormMemory memory;
    auto invalid=binary_observation(1,1);
    invalid.quality=std::numeric_limits<double>::quiet_NaN();
    const auto before=encoded(memory);
    THROWS(memory.apply(invalid,0));
    CHECK(encoded(memory)==before);
    invalid=binary_observation(1,1);
    invalid.source.origin=static_cast<NormOrigin>(255);
    THROWS(memory.apply(invalid,0));
    CHECK(encoded(memory)==before);
}

TEST("norm_memory", nm005_boundary_value_types_round_trip_without_world_state) {
    static_assert(!std::is_pointer_v<decltype(NormObservation::observed_actor)>);
    NormObservation observation=binary_observation(8,9,.25,.5,12);
    NormDecisionView view;
    view.mode=NormMode::Shadow;
    view.count=1;
    view.considered[0].key=observation.key;
    Writer writer;
    writer(observation,view);
    NormObservation loaded_observation;
    NormDecisionView loaded_view;
    Reader reader(writer.data);
    reader(loaded_observation,loaded_view);
    CHECK(reader.finished());
    CHECK(loaded_observation.key==observation.key);
    CHECK(loaded_observation.source.known_root==9);
    CHECK(loaded_view.count==1);
}

TEST("norm_memory", nm006_eight_of_ten_has_finite_uncertain_prediction) {
    BinaryNormEstimate estimate;
    estimate.positive=8;
    estimate.negative=2;
    NEAR(estimate.probability(),.75,1e-12);
    NEAR(estimate.coverage(),10./14,1e-12);
}

TEST("norm_memory", nm007_quality_is_multiplied_once) {
    NormMemory memory;
    auto observation=binary_observation(1,1);
    observation.quality=.8;
    observation.confidence=.7;
    observation.reliability=.6;
    observation.dose=.5;
    memory.apply(observation,0);
    const auto& estimate=memory.records().front().descriptive;
    NEAR(estimate.positive,.168,1e-12);
    NEAR(estimate.negative,0,1e-12);
}

TEST("norm_memory", nm008_duplicate_delivery_has_one_contribution_and_revision) {
    NormMemory memory;
    auto observation=binary_observation(1,1);
    CHECK(memory.apply(observation,0)==ApplyEvidence::Added);
    const auto revision=memory.revision();
    CHECK(memory.apply(observation,0)==ApplyEvidence::Duplicate);
    CHECK(memory.revision()==revision);
    CHECK(memory.records().front().sources.size()==1);
}

TEST("norm_memory", nm009_known_root_duplicates_use_the_maximum_weight) {
    NormMemory memory;
    for(std::uint64_t delivery=1;delivery<=100;++delivery) {
        auto observation=binary_observation(delivery,77);
        observation.quality=delivery==50?.8:.4;
        memory.apply(observation,0);
    }
    const auto& record=memory.records().front();
    CHECK(record.sources.size()==1);
    NEAR(record.descriptive.positive,.8,1e-12);
}

TEST("norm_memory", nm010_hidden_source_graph_cannot_change_projected_belief) {
    NormMemory first,second;
    auto observation=binary_observation(1,0);
    observation.source.independently_grounded=false;
    first.apply(observation,0);
    second.apply(observation,0);
    const auto a=first.predict(key(),0);
    const auto b=second.predict(key(),0);
    NEAR(a.prevalence,b.prevalence,1e-12);
    NEAR(a.coverage,b.coverage,1e-12);
    CHECK(encoded(first)==encoded(second));
}

TEST("norm_memory", nm011_new_revision_replaces_instead_of_doubling_a_root) {
    NormMemory memory;
    memory.apply(binary_observation(1,1,1),0);
    auto revised=binary_observation(2,1,0);
    revised.source.revision=2;
    CHECK(memory.apply(revised,0)==ApplyEvidence::Replaced);
    const auto& estimate=memory.records().front().descriptive;
    NEAR(estimate.positive,0,1e-12);
    NEAR(estimate.negative,1,1e-12);
    CHECK(memory.records().front().sources.size()==1);
}

TEST("norm_memory", nm012_same_revision_with_different_meaning_is_rejected_strongly) {
    NormMemory memory;
    memory.apply(binary_observation(1,1,1),0);
    auto conflicting=binary_observation(2,1,0);
    const auto before=encoded(memory);
    THROWS(memory.apply(conflicting,0));
    CHECK(encoded(memory)==before);
}

TEST("norm_memory", nm013_split_exposure_matches_one_full_exposure) {
    NormMemory whole,split;
    whole.apply(binary_observation(1,0,1,1),0);
    for(std::uint64_t delivery=1;delivery<=300;++delivery) {
        auto part=binary_observation(delivery,0,1,1./300);
        part.source.independently_grounded=false;
        split.apply(part,0);
    }
    NEAR(whole.predict(key(),0).prevalence,split.predict(key(),0).prevalence,1e-12);
    NEAR(whole.predict(key(),0).coverage,split.predict(key(),0).coverage,1e-12);
}

TEST("norm_memory", nm014_actor_practice_context_day_cap_is_independent_of_variant) {
    NormMemory memory;
    auto first=binary_observation(1,1,1,1);
    memory.apply(first,0);
    auto second=binary_observation(2,2,1,1);
    second.key.variant=2;
    second.observed_actor=first.observed_actor;
    CHECK(memory.apply(second,0)==ApplyEvidence::Duplicate);
    CHECK(memory.size()==1);
}

TEST("norm_memory", nm015_empirical_weight_halves_without_turning_prior_into_evidence) {
    NormMemory memory;
    for(std::uint64_t root=1;root<=8;++root) {
        auto observation=binary_observation(root,root);
        observation.observed_actor=Id(root);
        memory.apply(observation,0);
    }
    const auto prediction=memory.predict(key(),30*game_day_ms);
    NEAR(prediction.prevalence,5./6,1e-12);
    NEAR(prediction.coverage,4./8,1e-12);
    CHECK(prediction.descriptive_known);
}

TEST("norm_memory", nm016_predict_is_a_pure_cached_read) {
    NormMemory memory;
    memory.apply(binary_observation(1,1),0);
    const auto before=encoded(memory);
    const auto revision=memory.revision();
    for(int i=0;i<100;++i) (void)memory.predict(key(),game_day_ms);
    CHECK(encoded(memory)==before);
    CHECK(memory.revision()==revision);
}

TEST("norm_memory", nm017_incomplete_reaction_window_is_unknown_not_indifference) {
    NormMemory memory;
    auto observation=binary_observation(1,1,0);
    observation.channel=NormChannel::Reaction;
    observation.outcome_window_complete=false;
    CHECK(memory.apply(observation,game_day_ms)==ApplyEvidence::Unknown);
    const auto prediction=memory.predict(key(),game_day_ms);
    CHECK(!prediction.sanction_known);
    CHECK(!prediction.approval_known);
}

TEST("norm_memory", nm018_old_known_root_is_not_rejuvenated_by_a_new_delivery) {
    NormMemory memory;
    memory.apply(binary_observation(1,1,1,1,0),0);
    auto retelling=binary_observation(2,1,1,1,30*game_day_ms);
    CHECK(memory.apply(retelling,30*game_day_ms)==ApplyEvidence::Duplicate);
    const auto prediction=memory.predict(key(),30*game_day_ms);
    NEAR(prediction.coverage,.5/4.5,1e-12);
    const auto recalled=memory.recall_evidence(key(),NormChannel::Descriptive,30*game_day_ms);
    CHECK(recalled.has_value());
    CHECK(recalled->at==0);
    CHECK(recalled->source.known_root==1&&recalled->source.revision==1);
    NEAR(recalled->quality,1,1e-12);
    NEAR(recalled->reliability,1,1e-12);
}

TEST("norm_memory", nm042_full_personal_principle_is_a_finite_cost) {
    NormMemory memory;
    CHECK(memory.seed_principle(key(),1,source(0,51,1,NormOrigin::LegacyPrior),0)==ApplyEvidence::Added);
    const auto prediction=memory.predict(key(),0);
    CHECK(prediction.personal_principle_known);
    CHECK(prediction.personal_origin==NormOrigin::LegacyPrior);
    CHECK(std::isfinite(prediction.personal_resistance));
    NEAR(prediction.personal_resistance,1,1e-12);
}

TEST("norm_memory", nm043_personal_principle_does_not_require_an_audience) {
    NormMemory memory;
    auto private_key=key();
    private_key.local_group=0;
    memory.seed_principle(private_key,.8,source(0,52,1,NormOrigin::Historical),0);
    CHECK(!memory.predict(private_key,0).approval_known);
    NEAR(memory.predict(private_key,0).personal_resistance,.8,1e-12);
}

TEST("norm_memory", nm044_unknown_permission_is_not_proven_absence_of_permission) {
    NormMemory memory;
    const auto prediction=memory.predict(key(),0);
    CHECK(!prediction.personal_principle_known);
    NEAR(prediction.personal_resistance,0,1e-12);
}

TEST("norm_memory", nm045_exception_changes_qualification_without_weakening_principle) {
    NormMemory memory;
    memory.seed_principle(key(),.8,source(0,53,1,NormOrigin::LegacyPrior),0);
    const auto before=memory.predict(key(),0);
    CHECK(memory.qualify_principle(key(),1,1,1,91,source(9,90,1),1)==ApplyEvidence::Replaced);
    const auto after=memory.predict(key(),1);
    NEAR(after.principle_weight,before.principle_weight,1e-12);
    NEAR(after.personal_resistance,0,1e-12);
}

TEST("norm_memory", nm046_descriptive_frequency_never_updates_personal_weight) {
    NormMemory memory;
    memory.seed_principle(key(),.6,source(0,54,1,NormOrigin::LegacyPrior),0);
    for(std::uint64_t root=1;root<=20;++root) {
        auto observation=binary_observation(root,root,0);
        observation.observed_actor=Id(root);
        memory.apply(observation,0);
    }
    NEAR(memory.predict(key(),0).principle_weight,.6,1e-12);
}

TEST("norm_memory", nm047_only_an_explicitly_accepted_argument_updates_principle) {
    NormMemory memory;
    memory.seed_principle(key(),.8,source(0,55,1,NormOrigin::Historical),0);
    auto ignored=principle_argument(1,60,.2);
    ignored.accepted_argument=false;
    CHECK(memory.apply(ignored,0)==ApplyEvidence::Unknown);
    NEAR(memory.predict(key(),0).principle_weight,.8,1e-12);
    CHECK(memory.apply(principle_argument(2,61,.2),0)==ApplyEvidence::Added);
    NEAR(memory.predict(key(),0).principle_weight,.7940299002495008,1e-12);
}

TEST("norm_memory", personal_cost_uses_the_maximum_variant_per_aspect) {
    NormMemory memory;
    auto first=key(1,1);
    auto second=key(1,2);
    memory.seed_principle(first,.8,source(0,70,1,NormOrigin::Historical),0);
    memory.seed_principle(second,.7,source(0,71,1,NormOrigin::Historical),0);
    memory.qualify_principle(first,1,0,1,100,source(3,72),0);
    memory.qualify_principle(second,1,0,1,100,source(4,73),0);
    const std::array keys{first,second};
    NEAR(memory.personal_resistance(keys,0),.8,1e-12);
}

TEST("norm_memory", cold_records_survive_the_hot_cache_limit) {
    NormMemory memory;
    for(std::uint32_t practice=1;practice<=129;++practice) {
        auto observation=binary_observation(practice,practice);
        observation.key=key(practice);
        if(practice<129) {
            observation.key.local_group=0;
            observation.key.context=0;
        } else {
            observation.key.local_group=99;
            observation.key.context=88;
        }
        observation.observed_actor=practice;
        memory.apply(observation,0);
    }
    CHECK(memory.size()==129);
    for(const auto& record:memory.records())
        CHECK(memory.predict(record.key,0).descriptive_known);
    CHECK(memory.hot_cache_size()==128);
    CHECK(memory.keys(0,999).size()==32);
    auto late=memory.records().back().key;
    const auto contextual=memory.keys_for_context(late.local_group,late.context,999);
    CHECK(std::find(contextual.begin(),contextual.end(),late)!=contextual.end());
}

TEST("norm_memory", principle_revision_replays_ordered_transforms_at_original_time) {
    NormMemory memory;
    memory.seed_principle(key(),.8,source(0,80,1,NormOrigin::Historical),0);
    memory.apply(principle_argument(1,81,.2,10),20);
    memory.apply(principle_argument(2,82,1,20),20);
    auto revised=principle_argument(3,81,.9,999);
    revised.source.revision=2;
    CHECK(memory.apply(revised,1000)==ApplyEvidence::Replaced);
    const auto& principle=memory.records().front().personal;
    CHECK(principle.transforms.size()==2);
    CHECK(principle.transforms[0].at==10);
    CHECK(principle.transforms[1].at==20);
    CHECK(principle.checkpoints.size()>=1);
    const double alpha=1-std::exp(-.01);
    const double expected=(.8+alpha*(.9-.8))+alpha*(1-(.8+alpha*(.9-.8)));
    NEAR(principle.weight,expected,1e-12);
}

TEST("norm_memory", serialized_records_restore_without_serializing_hot_cache) {
    NormMemory memory;
    memory.apply(binary_observation(1,1),0);
    (void)memory.predict(key(),0);
    CHECK(memory.hot_cache_size()==1);
    Writer writer;
    writer(memory);
    NormMemory loaded;
    Reader reader(writer.data);
    reader(loaded);
    CHECK(reader.finished());
    CHECK(loaded.hot_cache_size()==0);
    loaded.validate(0);
    CHECK(loaded.predict(key(),0).descriptive_known);
}

TEST("norm_memory", principle_changes_do_not_rejuvenate_group_evidence) {
    NormMemory memory;
    memory.apply(binary_observation(1,1),0);
    memory.seed_principle(key(),.8,source(0,99,1,NormOrigin::Historical),30*game_day_ms);
    const auto prediction=memory.predict(key(),30*game_day_ms);
    NEAR(prediction.coverage,.5/4.5,1e-12);
}

TEST("norm_memory", profile_defaults_and_enum_validation_are_explicit) {
    const NormProfile defaults;
    CHECK(defaults.mode==NormMode::Legacy);
    NEAR(defaults.half_life_days,30,1e-12);
    NEAR(defaults.coverage_kappa,4,1e-12);
    NEAR(defaults.personal_learning_factor,.02,1e-12);
    CHECK(defaults.inbox_limit==32&&defaults.hot_record_limit==128);
    CHECK(defaults.candidate_ids_limit==32&&defaults.deep_norm_limit==4);
    CHECK(defaults.group_limit==4&&defaults.new_option_limit==2);
    NormMemory memory;
    auto invalid=defaults;
    invalid.mode=static_cast<NormMode>(255);
    const auto before=encoded(memory);
    THROWS(memory.configure(invalid));
    CHECK(encoded(memory)==before);
}

TEST("norm_memory", sanction_requires_explicit_severity_and_uses_each_factor_once) {
    NormMemory memory;
    auto detection=binary_observation(1,1,.4);
    detection.channel=NormChannel::Detection;
    auto classification=binary_observation(2,2,.8);
    classification.channel=NormChannel::Classification;
    auto reaction=binary_observation(3,3,.5);
    reaction.channel=NormChannel::Reaction;
    reaction.outcome_severity_known=false;
    memory.apply(detection,0);
    memory.apply(classification,0);
    memory.apply(reaction,0);
    CHECK(!memory.predict(key(),0).sanction_known);
    reaction.source.delivery=4;
    reaction.source.revision=2;
    reaction.outcome_severity_known=true;
    reaction.outcome_severity=.6;
    memory.apply(reaction,0);
    const auto prediction=memory.predict(key(),0);
    CHECK(prediction.sanction_known&&prediction.severity_known);
    CHECK(prediction.sanction_cost().has_value());
    NEAR(*prediction.sanction_cost(),prediction.seen*prediction.classified*prediction.reacted*prediction.severity,1e-12);
    const auto recalled=memory.recall_evidence(key(),NormChannel::Reaction,0);
    CHECK(recalled.has_value()&&recalled->source.revision==2);
    CHECK(recalled->outcome_severity_known);
    NEAR(recalled->outcome_severity,.6,1e-12);
    NormPrediction exact;
    exact.sanction_known=exact.severity_known=true;
    exact.seen=.4;exact.classified=.8;exact.reacted=.5;exact.severity=.6;
    NEAR(*exact.sanction_cost(),.096,1e-12);
}

TEST("norm_memory", approval_only_evidence_has_its_own_coverage) {
    NormMemory memory;
    memory.apply(approval_observation(1,1,ApprovalValue::Disapprove),0);
    const auto prediction=memory.predict(key(),0);
    NEAR(prediction.coverage,0,1e-12);
    NEAR(prediction.approval_coverage,1./5,1e-12);
}

TEST("norm_memory", explicit_binary_and_approval_priors_preserve_authored_probabilities) {
    NormMemory memory;
    auto prior_source=source(0,500,1,NormOrigin::LegacyPrior);
    CHECK(memory.seed_binary(key(),NormChannel::Detection,.35,4,prior_source,0)==ApplyEvidence::Added);
    prior_source.known_root=501;
    CHECK(memory.seed_approval(key(),{0,.35,.65},4,prior_source,0)==ApplyEvidence::Added);
    const auto prediction=memory.predict(key(),0);
    CHECK(prediction.seen_known&&prediction.approval_known);
    NEAR(prediction.seen,.35,1e-12);
    NEAR(prediction.disapprove,.35,1e-12);
    NEAR(prediction.indifferent,.65,1e-12);
    NEAR(prediction.approval_coverage,.5,1e-12);
    CHECK(prediction.approval_origin==NormOrigin::LegacyPrior);
}

TEST("norm_memory", explicit_sanction_prior_requires_severity_provenance) {
    NormMemory memory;
    memory.seed_binary(key(),NormChannel::Detection,.4,4,source(0,510,1,NormOrigin::LegacyPrior),0);
    memory.seed_binary(key(),NormChannel::Classification,.8,4,source(0,511,1,NormOrigin::LegacyPrior),0);
    memory.seed_binary(key(),NormChannel::Reaction,.5,4,source(0,512,1,NormOrigin::LegacyPrior),0);
    CHECK(!memory.predict(key(),0).sanction_cost().has_value());
    memory.seed_severity(key(),.6,4,source(0,513,1,NormOrigin::LegacyPrior),0);
    const auto prediction=memory.predict(key(),0);
    CHECK(prediction.seen_known&&prediction.classified_known&&prediction.reacted_known&&prediction.severity_known);
    NEAR(*prediction.sanction_cost(),.096,1e-12);
}
