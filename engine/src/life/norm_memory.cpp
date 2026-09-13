#include "life/norm_memory.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace life {
namespace {

constexpr Tick day_ms=86'400'000;
constexpr std::size_t checkpoint_stride=16;

template<class E>
bool enum_at_most(E value,E last) {
    using U=std::underlying_type_t<E>;
    return static_cast<U>(value)<=static_cast<U>(last);
}

void require(bool condition,const char* message) {
    if(!condition) throw std::invalid_argument(message);
}

void require_finite(double value,const char* message) {
    require(std::isfinite(value),message);
}

void require_unit(double value,const char* message) {
    require_finite(value,message);
    require(value>=0&&value<=1,message);
}

void require_nonnegative(double value,const char* message) {
    require_finite(value,message);
    require(value>=0,message);
}

void validate_key(const NormKey& key_value) {
    require(key_value.practice!=0,"norm practice must be nonzero");
}

void validate_source(const NormSource& source_value,bool delivery_required) {
    require(enum_at_most(source_value.origin,NormOrigin::LegacyPrior),"invalid norm origin");
    require(source_value.revision!=0,"norm source revision must be nonzero");
    if(delivery_required) require(source_value.delivery!=0,"norm delivery must be nonzero");
    require(source_value.delivery!=0||source_value.known_root!=0,"norm source identity must be known");
}

void validate_observation(const NormObservation& observation,Tick now) {
    validate_key(observation.key);
    validate_source(observation.source,true);
    require(now>=0,"negative norm time");
    require(observation.at>=0&&observation.at<=now,"norm observation time outside history");
    require(enum_at_most(observation.channel,NormChannel::PersonalPrinciple),"invalid norm channel");
    require(enum_at_most(observation.approval,ApprovalValue::Indifferent),"invalid approval value");
    require_unit(observation.quality,"norm quality outside [0,1]");
    require_unit(observation.confidence,"norm confidence outside [0,1]");
    require_unit(observation.reliability,"norm reliability outside [0,1]");
    require_unit(observation.dose,"norm dose outside [0,1]");
    require_unit(observation.value,"norm value outside [0,1]");
    require_unit(observation.outcome_severity,"norm outcome severity outside [0,1]");
    require_unit(observation.plasticity,"norm plasticity outside [0,1]");
    require_unit(observation.principle_applicability,"principle applicability outside [0,1]");
    require_unit(observation.principle_exception,"principle exception outside [0,1]");
    require_unit(observation.principle_severity,"principle severity outside [0,1]");
}

std::size_t channel_index(NormChannel channel) {
    require(enum_at_most(channel,NormChannel::PersonalPrinciple),"invalid norm channel");
    return static_cast<std::size_t>(channel);
}

double decay_factor(Tick from,Tick to,double half_life_days) {
    require(from>=0&&to>=from,"invalid norm decay interval");
    const double days=double(to-from)/double(day_ms);
    return std::exp2(-days/half_life_days);
}

double evidence_weight(const NormObservation& observation,double admitted_dose) {
    return observation.quality*observation.confidence*observation.reliability*admitted_dose;
}

bool same_identity(const NormSource& left,const NormSource& right) {
    if(left.known_root!=0||right.known_root!=0)
        return left.known_root!=0&&left.known_root==right.known_root;
    if(!left.independently_grounded&&!right.independently_grounded) return true;
    return left.independently_grounded&&right.independently_grounded&&left.delivery==right.delivery;
}

std::uint64_t source_order(const NormSource& source) {
    return source.known_root!=0?source.known_root:source.delivery;
}

bool same_semantic_value(const NormEvidenceContribution& contribution,
                         const NormObservation& observation) {
    if(contribution.channel!=observation.channel) return false;
    if(observation.channel==NormChannel::Approval)
        return contribution.approval_value==observation.approval;
    if(observation.channel==NormChannel::Reaction&&
       (contribution.severity_known!=observation.outcome_severity_known||
        (contribution.severity_known&&contribution.severity_positive/
         (contribution.severity_positive+contribution.severity_negative)!=observation.outcome_severity)))
        return false;
    return contribution.value==observation.value;
}

auto record_position(std::vector<NormRecord>& records,const NormKey& key_value) {
    return std::lower_bound(records.begin(),records.end(),key_value,
        [](const NormRecord& record,const NormKey& value){return record.key<value;});
}

auto record_position(const std::vector<NormRecord>& records,const NormKey& key_value) {
    return std::lower_bound(records.begin(),records.end(),key_value,
        [](const NormRecord& record,const NormKey& value){return record.key<value;});
}

bool exposure_less(const NormExposureDose& left,const NormExposureDose& right) {
    return std::tie(left.observed_actor,left.practice,left.context,left.day)<
           std::tie(right.observed_actor,right.practice,right.context,right.day);
}

bool same_exposure(const NormExposureDose& left,const NormExposureDose& right) {
    return !exposure_less(left,right)&&!exposure_less(right,left);
}

void add_contribution(BinaryNormEstimate& estimate,const NormEvidenceContribution& source,Tick now,
                      double half_life_days) {
    const double factor=decay_factor(source.summary_at,now,half_life_days);
    estimate.positive+=source.positive*factor;
    estimate.negative+=source.negative*factor;
}

void rebuild_record_summary(NormRecord& record,Tick now,const NormProfile& profile) {
    auto clear_evidence=[](BinaryNormEstimate estimate) {
        estimate.positive=0;
        estimate.negative=0;
        return estimate;
    };
    record.descriptive=clear_evidence(record.descriptive);
    record.detection=clear_evidence(record.detection);
    record.classification=clear_evidence(record.classification);
    record.reaction=clear_evidence(record.reaction);
    record.severity=clear_evidence(record.severity);
    record.approval.evidence.fill(0);
    record.origin_known.fill(false);
    std::array<Tick,6> latest{};
    std::array<std::uint64_t,6> tie{};
    for(std::size_t i=0;i<record.prior_source_known.size();++i) if(record.prior_source_known[i]) {
        record.origin_known[i]=true;
        record.origins[i]=record.prior_sources[i].origin;
        latest[i]=record.prior_at[i];
        tie[i]=source_order(record.prior_sources[i]);
    }

    for(const auto& source:record.sources) {
        switch(source.channel) {
        case NormChannel::Descriptive:
            add_contribution(record.descriptive,source,now,profile.half_life_days);
            break;
        case NormChannel::Approval: {
            const double factor=decay_factor(source.summary_at,now,profile.half_life_days);
            for(std::size_t i=0;i<source.approval.size();++i)
                record.approval.evidence[i]+=source.approval[i]*factor;
            break;
        }
        case NormChannel::Detection:
            add_contribution(record.detection,source,now,profile.half_life_days);
            break;
        case NormChannel::Classification:
            add_contribution(record.classification,source,now,profile.half_life_days);
            break;
        case NormChannel::Reaction:
            add_contribution(record.reaction,source,now,profile.half_life_days);
            if(source.severity_known) {
                const double factor=decay_factor(source.summary_at,now,profile.half_life_days);
                record.severity.positive+=source.severity_positive*factor;
                record.severity.negative+=source.severity_negative*factor;
            }
            break;
        case NormChannel::PersonalPrinciple:
            throw std::logic_error("personal principle stored in evidence sources");
        }
        const auto index=channel_index(source.channel);
        const auto order=source_order(source.source);
        if(!record.origin_known[index]||source.original_at>latest[index]||
           (source.original_at==latest[index]&&order>tie[index])) {
            record.origin_known[index]=true;
            record.origins[index]=source.source.origin;
            latest[index]=source.original_at;
            tie[index]=order;
        }
    }
    record.summary_at=now;
}

void rebuild_principle(PersonalPrinciple& principle) {
    std::stable_sort(principle.transforms.begin(),principle.transforms.end(),
        [](const auto& left,const auto& right) {
            return std::tuple{left.at,source_order(left.source),left.source.revision,left.source.delivery}<
                   std::tuple{right.at,source_order(right.source),right.source.revision,right.source.delivery};
        });
    double weight=principle.has_explicit_prior?principle.prior_weight:0;
    principle.checkpoints.clear();
    principle.checkpoints.push_back({0,weight});
    for(std::size_t i=0;i<principle.transforms.size();++i) {
        const auto& transform=principle.transforms[i];
        weight+=transform.alpha*(transform.target-weight);
        if((i+1)%checkpoint_stride==0)
            principle.checkpoints.push_back({static_cast<std::uint32_t>(i+1),weight});
    }
    if(principle.checkpoints.back().transform_count!=principle.transforms.size())
        principle.checkpoints.push_back({static_cast<std::uint32_t>(principle.transforms.size()),weight});
    principle.weight=std::clamp(weight,0.,1.);
}

double raw_weight(const NormEvidenceContribution& source) {
    if(source.channel==NormChannel::Approval) {
        double result=0;
        for(double value:source.approval) result+=value;
        return result;
    }
    return source.positive+source.negative;
}

void set_binary_value(NormEvidenceContribution& contribution,double value,double weight) {
    contribution.value=value;
    contribution.positive=weight*value;
    contribution.negative=weight*(1-value);
    contribution.approval.fill(0);
}

void set_approval_value(NormEvidenceContribution& contribution,ApprovalValue value,double weight) {
    contribution.approval_value=value;
    contribution.value=0;
    contribution.positive=0;
    contribution.negative=0;
    contribution.approval.fill(0);
    contribution.approval[static_cast<std::size_t>(value)]=weight;
}

bool same_transform_value(const PersonalPrincipleTransform& transform,
                          const NormObservation& observation) {
    return transform.target==observation.value&&transform.plasticity==observation.plasticity;
}

void validate_estimate(const BinaryNormEstimate& estimate) {
    require_nonnegative(estimate.positive,"negative binary positive evidence");
    require_nonnegative(estimate.negative,"negative binary negative evidence");
    require_finite(estimate.prior_positive,"invalid binary positive prior");
    require_finite(estimate.prior_negative,"invalid binary negative prior");
    if(estimate.has_explicit_prior)
        require(estimate.prior_positive>=0&&estimate.prior_negative>=0&&
                estimate.prior_positive+estimate.prior_negative>0,
                "explicit binary prior must have positive strength");
    else
        require(estimate.prior_positive>0&&estimate.prior_negative>0,"binary priors must be positive");
}

void validate_approval(const ApprovalEstimate& estimate) {
    double total=0;
    for(double value:estimate.evidence) { require_nonnegative(value,"negative approval evidence"); total+=value; }
    (void)total;
    double prior_total=0;
    for(double value:estimate.prior) {
        require_finite(value,"invalid approval prior");
        require(value>=0,"approval priors must be nonnegative");
        prior_total+=value;
    }
    require(prior_total>0,"approval prior must have positive strength");
    if(!estimate.has_explicit_prior)
        for(double value:estimate.prior) require(value>0,"technical approval priors must be positive");
}

} // namespace

bool BinaryNormEstimate::known() const {
    validate_estimate(*this);
    return has_explicit_prior||positive+negative>0;
}

double BinaryNormEstimate::probability() const {
    validate_estimate(*this);
    return (prior_positive+positive)/(prior_positive+prior_negative+positive+negative);
}

double BinaryNormEstimate::coverage(double kappa) const {
    validate_estimate(*this);
    require_finite(kappa,"invalid coverage kappa");
    require(kappa>0,"coverage kappa must be positive");
    const double count=positive+negative+(has_explicit_prior?prior_positive+prior_negative:0);
    return count/(count+kappa);
}

bool ApprovalEstimate::known() const {
    validate_approval(*this);
    return has_explicit_prior||evidence[0]+evidence[1]+evidence[2]>0;
}

std::array<double,3> ApprovalEstimate::probabilities() const {
    validate_approval(*this);
    std::array<double,3> result{};
    const double total=prior[0]+prior[1]+prior[2]+evidence[0]+evidence[1]+evidence[2];
    for(std::size_t i=0;i<result.size();++i) result[i]=(prior[i]+evidence[i])/total;
    return result;
}

double ApprovalEstimate::coverage(double kappa) const {
    validate_approval(*this);
    require_finite(kappa,"invalid coverage kappa");
    require(kappa>0,"coverage kappa must be positive");
    const double count=evidence[0]+evidence[1]+evidence[2]+
        (has_explicit_prior?prior[0]+prior[1]+prior[2]:0);
    return count/(count+kappa);
}

std::optional<double> NormPrediction::sanction_cost() const {
    if(!sanction_known||!severity_known) return std::nullopt;
    require_unit(seen,"sanction detection outside [0,1]");
    require_unit(classified,"sanction classification outside [0,1]");
    require_unit(reacted,"sanction reaction outside [0,1]");
    require_unit(severity,"sanction severity outside [0,1]");
    return seen*classified*reacted*severity;
}

void NormProfile::validate() const {
    require(enum_at_most(mode,NormMode::NoNormDecisionEffects),"invalid norm mode");
    require_finite(half_life_days,"invalid norm half life");
    require(half_life_days>0,"norm half life must be positive");
    require_finite(coverage_kappa,"invalid norm coverage kappa");
    require(coverage_kappa>0,"norm coverage kappa must be positive");
    require_nonnegative(personal_learning_factor,"negative personal learning factor");
    require_unit(motive_on,"norm motive-on outside [0,1]");
    require_unit(motive_off,"norm motive-off outside [0,1]");
    require(motive_off<motive_on,"norm motive-off must be below motive-on");
    require(inbox_limit>0,"norm inbox limit must be positive");
    require(hot_record_limit>0,"norm hot record limit must be positive");
    require(candidate_ids_limit>0,"norm candidate index limit must be positive");
    require(deep_norm_limit>0&&deep_norm_limit<=4,"deep norm limit outside fixed view");
    require(group_limit>0&&group_limit<=4,"norm group limit outside fixed view");
    require(new_option_limit>0,"new norm option limit must be positive");
}

NormMemory::NormMemory() {
    profile_.validate();
}

NormMemory::NormMemory(NormProfile profile):profile_(std::move(profile)) {
    profile_.validate();
}

void NormMemory::configure(const NormProfile& profile) {
    profile.validate();
    if(profile==profile_) return;
    if(!records_.empty()) {
        require(profile.half_life_days==profile_.half_life_days,
                "cannot change norm half life after evidence");
        require(profile.coverage_kappa==profile_.coverage_kappa,
                "cannot change norm coverage after evidence");
        require(profile.personal_learning_factor==profile_.personal_learning_factor,
                "cannot change personal learning after evidence");
    }
    profile_=profile;
    ++revision_;
    prediction_cache_.clear();
    context_index_.clear();
}

ApplyEvidence NormMemory::apply(const NormObservation& observation,Tick now) {
    profile_.validate();
    validate_observation(observation,now);

    if(std::binary_search(deliveries_.begin(),deliveries_.end(),observation.source.delivery))
        return ApplyEvidence::Duplicate;
    if(!observation.applicable||!observation.value_known)
        return ApplyEvidence::Unknown;
    if(observation.channel==NormChannel::Reaction&&!observation.outcome_window_complete)
        return ApplyEvidence::Unknown;
    if(observation.channel==NormChannel::PersonalPrinciple&&!observation.accepted_argument)
        return ApplyEvidence::Unknown;
    if(evidence_weight(observation,observation.dose)<=0)
        return ApplyEvidence::Unknown;

    auto next_records=records_;
    auto next_deliveries=deliveries_;
    auto next_exposures=exposures_;
    auto delivery_position=std::lower_bound(next_deliveries.begin(),next_deliveries.end(),observation.source.delivery);

    auto record_it=record_position(next_records,observation.key);
    const bool record_exists=record_it!=next_records.end()&&record_it->key==observation.key;
    if(!record_exists) record_it=next_records.insert(record_it,NormRecord{.key=observation.key});
    else require(record_it->summary_at<=now,"norm update before record summary");

    if(observation.channel==NormChannel::PersonalPrinciple) {
        if(!record_it->sources.empty()) rebuild_record_summary(*record_it,now,profile_);
        else record_it->summary_at=now;
        auto& principle=record_it->personal;
        auto transform_it=std::find_if(principle.transforms.begin(),principle.transforms.end(),
            [&](const auto& transform){return same_identity(transform.source,observation.source);});
        const bool replacing=transform_it!=principle.transforms.end();
        double q=observation.quality*observation.confidence*observation.reliability;
        const double alpha=-std::expm1(-profile_.personal_learning_factor*observation.plasticity*q*observation.dose);
        PersonalPrincipleTransform updated{observation.source,observation.at,observation.value,alpha,
                                           q,observation.dose,observation.plasticity};
        ApplyEvidence result=ApplyEvidence::Added;
        if(replacing) {
            if(observation.source.revision<transform_it->source.revision) return ApplyEvidence::Rejected;
            if(observation.source.revision==transform_it->source.revision) {
                if(!same_transform_value(*transform_it,observation))
                    throw std::invalid_argument("different principle content at same revision");
                if(alpha<=transform_it->alpha) {
                    next_deliveries.insert(delivery_position,observation.source.delivery);
                    deliveries_=std::move(next_deliveries);
                    return ApplyEvidence::Duplicate;
                }
            }
            updated.at=transform_it->at;
            *transform_it=updated;
            result=ApplyEvidence::Replaced;
        } else {
            principle.transforms.push_back(updated);
        }
        principle.aspect=observation.principle_aspect!=0?observation.principle_aspect:observation.key.practice;
        principle.applicability=observation.principle_applicability;
        principle.exception=observation.principle_exception;
        principle.severity=observation.principle_severity;
        rebuild_principle(principle);
        ++principle.revision;
        ++record_it->revision;
        next_deliveries.insert(delivery_position,observation.source.delivery);
        records_=std::move(next_records);
        deliveries_=std::move(next_deliveries);
        exposures_=std::move(next_exposures);
        ++revision_;
        prediction_cache_.clear();
        context_index_.clear();
        return result;
    }

    auto source_it=std::find_if(record_it->sources.begin(),record_it->sources.end(),
        [&](const auto& contribution) {
            return contribution.channel==observation.channel&&same_identity(contribution.source,observation.source);
        });
    const bool source_exists=source_it!=record_it->sources.end();
    if(source_exists&&observation.source.known_root!=0) {
        if(observation.source.revision<source_it->source.revision) return ApplyEvidence::Rejected;
        if(observation.source.revision==source_it->source.revision) {
            if(!same_semantic_value(*source_it,observation))
                throw std::invalid_argument("different norm content at same revision");
            const double proposed=evidence_weight(observation,observation.dose);
            if(proposed<=raw_weight(*source_it)) {
                next_deliveries.insert(delivery_position,observation.source.delivery);
                deliveries_=std::move(next_deliveries);
                return ApplyEvidence::Duplicate;
            }
        }
    }

    double admitted_dose=observation.dose;
    // A known-root revision or stronger copy replaces its existing contribution.
    // It is not a second opportunity and therefore must not consume exposure dose.
    const bool replaces_known_source=source_exists&&observation.source.known_root!=0;
    if(observation.channel==NormChannel::Descriptive&&observation.observed_actor!=0&&
       !replaces_known_source) {
        NormExposureDose exposure{observation.observed_actor,observation.key.practice,
                                  observation.key.context,observation.at/day_ms,0};
        auto exposure_it=std::lower_bound(next_exposures.begin(),next_exposures.end(),exposure,exposure_less);
        if(exposure_it!=next_exposures.end()&&same_exposure(*exposure_it,exposure)) {
            admitted_dose=std::min(admitted_dose,1-exposure_it->dose);
            if(admitted_dose<=0) {
                next_deliveries.insert(delivery_position,observation.source.delivery);
                deliveries_=std::move(next_deliveries);
                return ApplyEvidence::Duplicate;
            }
            exposure_it->dose+=admitted_dose;
        } else {
            exposure.dose=admitted_dose;
            next_exposures.insert(exposure_it,exposure);
        }
    }

    const bool pooled=observation.source.known_root==0&&!observation.source.independently_grounded;
    if(source_exists&&pooled) {
        admitted_dose=std::min(admitted_dose,1-source_it->dose);
        if(admitted_dose<=0) {
            next_deliveries.insert(delivery_position,observation.source.delivery);
            deliveries_=std::move(next_deliveries);
            return ApplyEvidence::Duplicate;
        }
    }

    const double q=evidence_weight(observation,admitted_dose);
    ApplyEvidence result=source_exists?ApplyEvidence::Replaced:ApplyEvidence::Added;
    if(!source_exists) {
        record_it->sources.push_back({});
        source_it=std::prev(record_it->sources.end());
        source_it->channel=observation.channel;
        source_it->source=observation.source;
        source_it->original_at=observation.at;
        source_it->summary_at=observation.at;
        source_it->pooled=pooled;
    }

    if(source_exists&&pooled) {
        const double old_factor=decay_factor(source_it->summary_at,now,profile_.half_life_days);
        source_it->positive*=old_factor;
        source_it->negative*=old_factor;
        source_it->severity_positive*=old_factor;
        source_it->severity_negative*=old_factor;
        for(double& value:source_it->approval) value*=old_factor;
        const double new_factor=decay_factor(observation.at,now,profile_.half_life_days);
        if(observation.channel==NormChannel::Approval)
            source_it->approval[static_cast<std::size_t>(observation.approval)]+=q*new_factor;
        else {
            source_it->positive+=q*observation.value*new_factor;
            source_it->negative+=q*(1-observation.value)*new_factor;
            if(observation.channel==NormChannel::Reaction&&observation.outcome_severity_known) {
                source_it->severity_positive+=q*observation.outcome_severity*new_factor;
                source_it->severity_negative+=q*(1-observation.outcome_severity)*new_factor;
                source_it->severity_known=true;
            }
        }
        source_it->summary_at=now;
        source_it->original_at=std::min(source_it->original_at,observation.at);
        source_it->dose+=admitted_dose;
        source_it->source=observation.source;
    } else {
        const Tick original_at=source_exists?source_it->original_at:observation.at;
        source_it->source=observation.source;
        source_it->original_at=original_at;
        source_it->summary_at=original_at;
        source_it->dose=admitted_dose;
        if(observation.channel==NormChannel::Approval)
            set_approval_value(*source_it,observation.approval,q);
        else {
            set_binary_value(*source_it,observation.value,q);
            source_it->severity_positive=0;
            source_it->severity_negative=0;
            source_it->severity_known=observation.channel==NormChannel::Reaction&&observation.outcome_severity_known;
            if(source_it->severity_known) {
                source_it->severity_positive=q*observation.outcome_severity;
                source_it->severity_negative=q*(1-observation.outcome_severity);
            }
        }
    }
    source_it->value=observation.value;
    source_it->approval_value=observation.approval;
    source_it->quality=observation.quality;
    source_it->confidence=observation.confidence;
    source_it->reliability=observation.reliability;

    rebuild_record_summary(*record_it,now,profile_);
    ++record_it->revision;
    next_deliveries.insert(delivery_position,observation.source.delivery);
    records_=std::move(next_records);
    deliveries_=std::move(next_deliveries);
    exposures_=std::move(next_exposures);
    ++revision_;
    prediction_cache_.clear();
    context_index_.clear();
    return result;
}

ApplyEvidence NormMemory::seed_binary(const NormKey& key_value,NormChannel channel,
                                      double probability,double strength,
                                      const NormSource& source_value,Tick at) {
    profile_.validate();
    validate_key(key_value);
    channel_index(channel);
    require(channel==NormChannel::Descriptive||channel==NormChannel::Detection||
            channel==NormChannel::Classification||channel==NormChannel::Reaction,
            "binary prior requires a binary norm channel");
    require_unit(probability,"binary prior probability outside [0,1]");
    require_finite(strength,"invalid binary prior strength");
    require(strength>0,"binary prior strength must be positive");
    validate_source(source_value,false);
    require(source_value.origin==NormOrigin::Historical||source_value.origin==NormOrigin::Assumed||
            source_value.origin==NormOrigin::LegacyPrior,"binary seed requires explicit prior origin");
    require(at>=0,"negative binary prior time");

    auto next_records=records_;
    auto record_it=record_position(next_records,key_value);
    if(record_it==next_records.end()||record_it->key!=key_value)
        record_it=next_records.insert(record_it,NormRecord{.key=key_value});
    const auto index=channel_index(channel);
    BinaryNormEstimate* estimate=nullptr;
    switch(channel) {
    case NormChannel::Descriptive: estimate=&record_it->descriptive;break;
    case NormChannel::Detection: estimate=&record_it->detection;break;
    case NormChannel::Classification: estimate=&record_it->classification;break;
    case NormChannel::Reaction: estimate=&record_it->reaction;break;
    default: throw std::logic_error("non-binary prior channel");
    }
    ApplyEvidence result=ApplyEvidence::Added;
    if(record_it->prior_source_known[index]) {
        require(same_identity(record_it->prior_sources[index],source_value),"different binary prior source");
        if(source_value.revision<record_it->prior_sources[index].revision) return ApplyEvidence::Rejected;
        if(source_value.revision==record_it->prior_sources[index].revision) {
            if(estimate->prior_positive!=probability*strength||
               estimate->prior_negative!=(1-probability)*strength)
                throw std::invalid_argument("different binary prior at same revision");
            return ApplyEvidence::Duplicate;
        }
        at=record_it->prior_at[index];
        result=ApplyEvidence::Replaced;
    }
    if(at>record_it->summary_at) rebuild_record_summary(*record_it,at,profile_);
    estimate->prior_positive=probability*strength;
    estimate->prior_negative=(1-probability)*strength;
    estimate->has_explicit_prior=true;
    record_it->prior_sources[index]=source_value;
    record_it->prior_at[index]=at;
    record_it->prior_source_known[index]=true;
    record_it->origins[index]=source_value.origin;
    record_it->origin_known[index]=true;
    ++record_it->revision;
    records_=std::move(next_records);
    ++revision_;
    prediction_cache_.clear();
    context_index_.clear();
    return result;
}

ApplyEvidence NormMemory::seed_approval(const NormKey& key_value,
                                        const std::array<double,3>& probabilities,
                                        double strength,const NormSource& source_value,Tick at) {
    profile_.validate();
    validate_key(key_value);
    double total=0;
    for(double value:probabilities) { require_unit(value,"approval prior probability outside [0,1]"); total+=value; }
    require(std::abs(total-1)<=1e-12,"approval prior probabilities must sum to one");
    require_finite(strength,"invalid approval prior strength");
    require(strength>0,"approval prior strength must be positive");
    validate_source(source_value,false);
    require(source_value.origin==NormOrigin::Historical||source_value.origin==NormOrigin::Assumed||
            source_value.origin==NormOrigin::LegacyPrior,"approval seed requires explicit prior origin");
    require(at>=0,"negative approval prior time");

    auto next_records=records_;
    auto record_it=record_position(next_records,key_value);
    if(record_it==next_records.end()||record_it->key!=key_value)
        record_it=next_records.insert(record_it,NormRecord{.key=key_value});
    const auto index=channel_index(NormChannel::Approval);
    ApplyEvidence result=ApplyEvidence::Added;
    if(record_it->prior_source_known[index]) {
        require(same_identity(record_it->prior_sources[index],source_value),"different approval prior source");
        if(source_value.revision<record_it->prior_sources[index].revision) return ApplyEvidence::Rejected;
        if(source_value.revision==record_it->prior_sources[index].revision) {
            bool same=true;
            for(std::size_t i=0;i<probabilities.size();++i)
                same&=record_it->approval.prior[i]==probabilities[i]*strength;
            if(!same) throw std::invalid_argument("different approval prior at same revision");
            return ApplyEvidence::Duplicate;
        }
        at=record_it->prior_at[index];
        result=ApplyEvidence::Replaced;
    }
    if(at>record_it->summary_at) rebuild_record_summary(*record_it,at,profile_);
    for(std::size_t i=0;i<probabilities.size();++i)
        record_it->approval.prior[i]=probabilities[i]*strength;
    record_it->approval.has_explicit_prior=true;
    record_it->prior_sources[index]=source_value;
    record_it->prior_at[index]=at;
    record_it->prior_source_known[index]=true;
    record_it->origins[index]=source_value.origin;
    record_it->origin_known[index]=true;
    ++record_it->revision;
    records_=std::move(next_records);
    ++revision_;
    prediction_cache_.clear();
    context_index_.clear();
    return result;
}

ApplyEvidence NormMemory::seed_severity(const NormKey& key_value,double severity,double strength,
                                        const NormSource& source_value,Tick at) {
    profile_.validate();
    validate_key(key_value);
    require_unit(severity,"severity prior outside [0,1]");
    require_finite(strength,"invalid severity prior strength");
    require(strength>0,"severity prior strength must be positive");
    validate_source(source_value,false);
    require(source_value.origin==NormOrigin::Historical||source_value.origin==NormOrigin::Assumed||
            source_value.origin==NormOrigin::LegacyPrior,"severity seed requires explicit prior origin");
    require(at>=0,"negative severity prior time");

    auto next_records=records_;
    auto record_it=record_position(next_records,key_value);
    if(record_it==next_records.end()||record_it->key!=key_value)
        record_it=next_records.insert(record_it,NormRecord{.key=key_value});
    ApplyEvidence result=ApplyEvidence::Added;
    if(record_it->severity_prior_source_known) {
        require(same_identity(record_it->severity_prior_source,source_value),"different severity prior source");
        if(source_value.revision<record_it->severity_prior_source.revision) return ApplyEvidence::Rejected;
        if(source_value.revision==record_it->severity_prior_source.revision) {
            if(record_it->severity.prior_positive!=severity*strength||
               record_it->severity.prior_negative!=(1-severity)*strength)
                throw std::invalid_argument("different severity prior at same revision");
            return ApplyEvidence::Duplicate;
        }
        at=record_it->severity_prior_at;
        result=ApplyEvidence::Replaced;
    }
    if(at>record_it->summary_at) rebuild_record_summary(*record_it,at,profile_);
    record_it->severity.prior_positive=severity*strength;
    record_it->severity.prior_negative=(1-severity)*strength;
    record_it->severity.has_explicit_prior=true;
    record_it->severity_prior_source=source_value;
    record_it->severity_prior_at=at;
    record_it->severity_prior_source_known=true;
    ++record_it->revision;
    records_=std::move(next_records);
    ++revision_;
    prediction_cache_.clear();
    context_index_.clear();
    return result;
}

ApplyEvidence NormMemory::seed_principle(const NormKey& key_value,double value,
                                         const NormSource& source_value,Tick at) {
    profile_.validate();
    validate_key(key_value);
    validate_source(source_value,false);
    require_unit(value,"principle prior outside [0,1]");
    require(at>=0,"negative principle prior time");
    require(source_value.origin==NormOrigin::Historical||source_value.origin==NormOrigin::LegacyPrior,
            "principle seed requires historical or legacy origin");

    auto next_records=records_;
    auto record_it=record_position(next_records,key_value);
    const bool exists=record_it!=next_records.end()&&record_it->key==key_value;
    if(!exists) record_it=next_records.insert(record_it,NormRecord{.key=key_value});
    auto& principle=record_it->personal;
    ApplyEvidence result=ApplyEvidence::Added;
    if(principle.has_explicit_prior) {
        require(same_identity(principle.prior_source,source_value),"different principle prior source");
        if(source_value.revision<principle.prior_source.revision) return ApplyEvidence::Rejected;
        if(source_value.revision==principle.prior_source.revision) {
            if(value!=principle.prior_weight)
                throw std::invalid_argument("different principle prior at same revision");
            return ApplyEvidence::Duplicate;
        }
        result=ApplyEvidence::Replaced;
        at=principle.prior_at;
    }
    principle.has_explicit_prior=true;
    principle.prior_weight=value;
    principle.prior_source=source_value;
    principle.prior_at=at;
    if(principle.aspect==0) principle.aspect=key_value.practice;
    if(at>record_it->summary_at) {
        if(!record_it->sources.empty()) rebuild_record_summary(*record_it,at,profile_);
        else record_it->summary_at=at;
    }
    rebuild_principle(principle);
    ++principle.revision;
    ++record_it->revision;
    records_=std::move(next_records);
    ++revision_;
    prediction_cache_.clear();
    context_index_.clear();
    return result;
}

ApplyEvidence NormMemory::qualify_principle(const NormKey& key_value,double applicability,
                                            double exception,double severity,std::uint32_t aspect,
                                            const NormSource& source_value,Tick at) {
    profile_.validate();
    validate_key(key_value);
    validate_source(source_value,false);
    require_unit(applicability,"principle applicability outside [0,1]");
    require_unit(exception,"principle exception outside [0,1]");
    require_unit(severity,"principle severity outside [0,1]");
    require(at>=0,"negative principle qualification time");

    auto next_records=records_;
    auto record_it=record_position(next_records,key_value);
    if(record_it==next_records.end()||record_it->key!=key_value||
       (!record_it->personal.has_explicit_prior&&record_it->personal.transforms.empty()))
        return ApplyEvidence::Unknown;
    auto& principle=record_it->personal;
    if(principle.qualification_source.revision!=0&&same_identity(principle.qualification_source,source_value)) {
        if(source_value.revision<principle.qualification_source.revision) return ApplyEvidence::Rejected;
        if(source_value.revision==principle.qualification_source.revision) {
            if(principle.applicability!=applicability||principle.exception!=exception||
               principle.severity!=severity||principle.aspect!=(aspect!=0?aspect:key_value.practice))
                throw std::invalid_argument("different principle qualification at same revision");
            return ApplyEvidence::Duplicate;
        }
        at=principle.qualification_at;
    }
    principle.applicability=applicability;
    principle.exception=exception;
    principle.severity=severity;
    principle.aspect=aspect!=0?aspect:key_value.practice;
    principle.qualification_source=source_value;
    principle.qualification_at=at;
    if(at>record_it->summary_at) {
        if(!record_it->sources.empty()) rebuild_record_summary(*record_it,at,profile_);
        else record_it->summary_at=at;
    }
    ++principle.revision;
    ++record_it->revision;
    records_=std::move(next_records);
    ++revision_;
    prediction_cache_.clear();
    context_index_.clear();
    return ApplyEvidence::Replaced;
}

NormPrediction NormMemory::predict(const NormKey& key_value,Tick now) const {
    profile_.validate();
    validate_key(key_value);
    require(now>=0,"negative norm prediction time");
    auto record_it=record_position(records_,key_value);
    if(record_it==records_.end()||record_it->key!=key_value) {
        NormPrediction unknown;
        unknown.key=key_value;
        unknown.prevalence=.5;
        unknown.approve=unknown.disapprove=unknown.indifferent=1./3;
        unknown.seen=unknown.classified=unknown.reacted=.5;
        return unknown;
    }
    require(record_it->summary_at<=now,"norm prediction before record summary");
    auto cached=std::find_if(prediction_cache_.begin(),prediction_cache_.end(),
        [&](const auto& entry) {
            return entry.key==key_value&&entry.at==now&&entry.record_revision==record_it->revision;
        });
    if(cached!=prediction_cache_.end()) return cached->prediction;

    const double factor=decay_factor(record_it->summary_at,now,profile_.half_life_days);
    auto decayed=[&](BinaryNormEstimate estimate) {
        estimate.positive*=factor;
        estimate.negative*=factor;
        return estimate;
    };
    const auto descriptive=decayed(record_it->descriptive);
    const auto detection=decayed(record_it->detection);
    const auto classification=decayed(record_it->classification);
    const auto reaction=decayed(record_it->reaction);
    const auto severity=decayed(record_it->severity);
    auto approval=record_it->approval;
    for(double& value:approval.evidence) value*=factor;

    NormPrediction prediction;
    prediction.key=key_value;
    prediction.descriptive_known=descriptive.known();
    prediction.approval_known=approval.known();
    prediction.prevalence=descriptive.probability();
    prediction.coverage=descriptive.coverage(profile_.coverage_kappa);
    const auto probabilities=approval.probabilities();
    prediction.approve=probabilities[0];
    prediction.disapprove=probabilities[1];
    prediction.indifferent=probabilities[2];
    prediction.approval_coverage=approval.coverage(profile_.coverage_kappa);
    prediction.seen=detection.probability();
    prediction.classified=classification.probability();
    prediction.reacted=reaction.probability();
    prediction.severity=severity.probability();
    prediction.seen_known=detection.known();
    prediction.classified_known=classification.known();
    prediction.reacted_known=reaction.known();
    prediction.severity_known=severity.known();
    prediction.sanction_known=prediction.seen_known&&prediction.classified_known&&
                              prediction.reacted_known&&
                              prediction.severity_known;
    prediction.personal_principle_known=record_it->personal.has_explicit_prior||
                                        !record_it->personal.transforms.empty();
    prediction.principle_weight=record_it->personal.weight;
    prediction.personal_applicability=record_it->personal.applicability;
    prediction.personal_exception=record_it->personal.exception;
    prediction.personal_severity=record_it->personal.severity;
    prediction.personal_resistance=std::clamp(prediction.principle_weight*
        prediction.personal_applicability*prediction.personal_severity*
        (1-prediction.personal_exception),0.,1.);
    if(record_it->origin_known[channel_index(NormChannel::Descriptive)])
        prediction.descriptive_origin=record_it->origins[channel_index(NormChannel::Descriptive)];
    if(record_it->origin_known[channel_index(NormChannel::Approval)])
        prediction.approval_origin=record_it->origins[channel_index(NormChannel::Approval)];
    if(record_it->origin_known[channel_index(NormChannel::Reaction)])
        prediction.sanction_origin=record_it->origins[channel_index(NormChannel::Reaction)];
    if(!record_it->personal.transforms.empty())
        prediction.personal_origin=record_it->personal.transforms.back().source.origin;
    else if(record_it->personal.has_explicit_prior)
        prediction.personal_origin=record_it->personal.prior_source.origin;
    prediction.own_revision=record_it->revision;

    if(prediction_cache_.size()>=profile_.hot_record_limit) prediction_cache_.erase(prediction_cache_.begin());
    prediction_cache_.push_back({key_value,now,record_it->revision,prediction});
    return prediction;
}

std::optional<NormObservation> NormMemory::recall_evidence(const NormKey& key_value,
                                                           NormChannel channel,Tick now) const {
    validate_key(key_value);
    channel_index(channel);
    require(now>=0,"negative norm recall time");
    auto record_it=record_position(records_,key_value);
    if(record_it==records_.end()||record_it->key!=key_value) return std::nullopt;
    if(channel==NormChannel::PersonalPrinciple) {
        NormObservation result;
        result.key=key_value;
        result.channel=channel;
        result.applicable=true;
        result.value_known=true;
        result.outcome_window_complete=true;
        result.accepted_argument=true;
        result.principle_aspect=record_it->personal.aspect;
        result.principle_applicability=record_it->personal.applicability;
        result.principle_exception=record_it->personal.exception;
        result.principle_severity=record_it->personal.severity;
        if(!record_it->personal.transforms.empty()) {
            const auto& transform=record_it->personal.transforms.back();
            if(transform.at>now) return std::nullopt;
            result.source=transform.source;
            result.at=transform.at;
            result.value=transform.target;
            result.quality=transform.quality;
            result.confidence=1;
            result.reliability=1;
            result.dose=transform.dose;
            result.plasticity=transform.plasticity;
            return result;
        }
        if(!record_it->personal.has_explicit_prior||record_it->personal.prior_at>now) return std::nullopt;
        result.source=record_it->personal.prior_source;
        result.at=record_it->personal.prior_at;
        result.value=record_it->personal.prior_weight;
        result.quality=result.confidence=result.reliability=result.dose=1;
        return result;
    }

    const NormEvidenceContribution* latest=nullptr;
    for(const auto& contribution:record_it->sources) {
        if(contribution.channel!=channel||contribution.original_at>now) continue;
        if(latest==nullptr||contribution.original_at>latest->original_at||
           (contribution.original_at==latest->original_at&&source_order(contribution.source)>source_order(latest->source)))
            latest=&contribution;
    }
    if(latest==nullptr) return std::nullopt;
    NormObservation result;
    result.key=key_value;
    result.source=latest->source;
    result.at=latest->original_at;
    result.quality=latest->quality;
    result.confidence=latest->confidence;
    result.reliability=latest->reliability;
    result.dose=latest->dose;
    result.value=latest->value;
    result.channel=latest->channel;
    result.approval=latest->approval_value;
    result.outcome_severity_known=latest->severity_known;
    if(latest->severity_known)
        result.outcome_severity=latest->severity_positive/
                                (latest->severity_positive+latest->severity_negative);
    result.applicable=true;
    result.value_known=true;
    result.outcome_window_complete=channel==NormChannel::Reaction;
    return result;
}

double NormMemory::personal_resistance(std::span<const NormKey> keys_value,Tick now) const {
    std::vector<std::pair<std::uint32_t,double>> aspects;
    for(const auto& key_value:keys_value) {
        const auto prediction=predict(key_value,now);
        if(!prediction.personal_principle_known) continue;
        auto record_it=record_position(records_,key_value);
        const auto aspect=record_it->personal.aspect!=0?record_it->personal.aspect:key_value.practice;
        auto found=std::find_if(aspects.begin(),aspects.end(),
            [&](const auto& entry){return entry.first==aspect;});
        if(found==aspects.end()) aspects.push_back({aspect,prediction.personal_resistance});
        else found->second=std::max(found->second,prediction.personal_resistance);
    }
    double total=0;
    for(const auto& entry:aspects) total+=entry.second;
    return std::clamp(total,0.,1.);
}

void NormMemory::validate(Tick now) const {
    profile_.validate();
    require(now>=0,"negative norm validation time");
    require(std::is_sorted(records_.begin(),records_.end(),
        [](const auto& left,const auto& right){return left.key<right.key;}),"unsorted norm records");
    for(std::size_t i=1;i<records_.size();++i)
        require(records_[i-1].key!=records_[i].key,"duplicate norm record");
    require(std::is_sorted(deliveries_.begin(),deliveries_.end()),"unsorted norm deliveries");
    require(std::adjacent_find(deliveries_.begin(),deliveries_.end())==deliveries_.end(),
            "duplicate norm delivery");
    for(auto delivery:deliveries_) require(delivery!=0,"zero norm delivery");
    require(std::is_sorted(exposures_.begin(),exposures_.end(),exposure_less),"unsorted norm exposures");
    for(std::size_t i=1;i<exposures_.size();++i)
        require(!same_exposure(exposures_[i-1],exposures_[i]),"duplicate norm exposure");
    for(const auto& exposure:exposures_) {
        require(exposure.observed_actor!=0&&exposure.practice!=0,"invalid norm exposure key");
        require_unit(exposure.dose,"norm exposure dose outside [0,1]");
    }
    for(const auto& record:records_) {
        validate_key(record.key);
        require(record.summary_at>=0&&record.summary_at<=now,"norm record time outside history");
        require(record.revision!=0,"zero norm record revision");
        validate_estimate(record.descriptive);
        validate_estimate(record.detection);
        validate_estimate(record.classification);
        validate_estimate(record.reaction);
        validate_estimate(record.severity);
        validate_approval(record.approval);
        require(record.descriptive.has_explicit_prior==
                record.prior_source_known[channel_index(NormChannel::Descriptive)],
                "descriptive prior provenance mismatch");
        require(record.approval.has_explicit_prior==
                record.prior_source_known[channel_index(NormChannel::Approval)],
                "approval prior provenance mismatch");
        require(record.detection.has_explicit_prior==
                record.prior_source_known[channel_index(NormChannel::Detection)],
                "detection prior provenance mismatch");
        require(record.classification.has_explicit_prior==
                record.prior_source_known[channel_index(NormChannel::Classification)],
                "classification prior provenance mismatch");
        require(record.reaction.has_explicit_prior==
                record.prior_source_known[channel_index(NormChannel::Reaction)],
                "reaction prior provenance mismatch");
        require(record.severity.has_explicit_prior==record.severity_prior_source_known,
                "severity prior provenance mismatch");
        for(std::size_t i=0;i<record.origins.size();++i)
            if(record.origin_known[i]) require(enum_at_most(record.origins[i],NormOrigin::LegacyPrior),"invalid record origin");
        for(std::size_t i=0;i<record.prior_source_known.size();++i) {
            if(!record.prior_source_known[i]) continue;
            validate_source(record.prior_sources[i],false);
            require(record.prior_sources[i].origin==NormOrigin::Historical||
                    record.prior_sources[i].origin==NormOrigin::Assumed||
                    record.prior_sources[i].origin==NormOrigin::LegacyPrior,
                    "invalid explicit prior origin");
            require(record.prior_at[i]>=0&&record.prior_at[i]<=now,
                    "explicit prior time outside history");
            if(i==channel_index(NormChannel::Approval))
                require(record.approval.has_explicit_prior,"missing approval explicit prior");
            else if(i!=channel_index(NormChannel::PersonalPrinciple)) {
                const BinaryNormEstimate* estimate=nullptr;
                switch(static_cast<NormChannel>(i)) {
                case NormChannel::Descriptive: estimate=&record.descriptive;break;
                case NormChannel::Detection: estimate=&record.detection;break;
                case NormChannel::Classification: estimate=&record.classification;break;
                case NormChannel::Reaction: estimate=&record.reaction;break;
                default: break;
                }
                require(estimate!=nullptr&&estimate->has_explicit_prior,
                        "missing binary explicit prior");
            }
        }
        require(!record.prior_source_known[channel_index(NormChannel::PersonalPrinciple)],
                "personal prior stored in channel prior ledger");
        if(record.severity_prior_source_known) {
            validate_source(record.severity_prior_source,false);
            require(record.severity_prior_source.origin==NormOrigin::Historical||
                    record.severity_prior_source.origin==NormOrigin::Assumed||
                    record.severity_prior_source.origin==NormOrigin::LegacyPrior,
                    "invalid severity prior origin");
            require(record.severity_prior_at>=0&&record.severity_prior_at<=now,
                    "severity prior time outside history");
            require(record.severity.has_explicit_prior,"missing severity explicit prior");
        }
        for(const auto& contribution:record.sources) {
            require(contribution.channel!=NormChannel::PersonalPrinciple,"personal contribution in source ledger");
            channel_index(contribution.channel);
            validate_source(contribution.source,true);
            require(contribution.original_at>=0&&contribution.original_at<=now,"source origin time outside history");
            require(contribution.summary_at>=contribution.original_at&&contribution.summary_at<=now,
                    "source summary time outside history");
            require_nonnegative(contribution.positive,"negative source positive evidence");
            require_nonnegative(contribution.negative,"negative source negative evidence");
            require_nonnegative(contribution.severity_positive,"negative source severity evidence");
            require_nonnegative(contribution.severity_negative,"negative source severity evidence");
            require(contribution.severity_known==
                    (contribution.severity_positive+contribution.severity_negative>0),
                    "inconsistent source severity evidence");
            for(double value:contribution.approval) require_nonnegative(value,"negative source approval evidence");
            require_unit(contribution.dose,"source dose outside [0,1]");
            require_unit(contribution.value,"source value outside [0,1]");
            require_unit(contribution.quality,"source quality outside [0,1]");
            require_unit(contribution.confidence,"source confidence outside [0,1]");
            require_unit(contribution.reliability,"source reliability outside [0,1]");
            require(enum_at_most(contribution.approval_value,ApprovalValue::Indifferent),"invalid source approval");
        }
        const auto& principle=record.personal;
        require_unit(principle.prior_weight,"principle prior outside [0,1]");
        require_unit(principle.applicability,"principle applicability outside [0,1]");
        require_unit(principle.exception,"principle exception outside [0,1]");
        require_unit(principle.severity,"principle severity outside [0,1]");
        require_unit(principle.weight,"principle weight outside [0,1]");
        if(principle.has_explicit_prior) {
            validate_source(principle.prior_source,false);
            require(principle.prior_at>=0&&principle.prior_at<=now,"principle prior time outside history");
        }
        require(std::is_sorted(principle.transforms.begin(),principle.transforms.end(),
            [](const auto& left,const auto& right) {
                return std::tuple{left.at,source_order(left.source),left.source.revision,left.source.delivery}<
                       std::tuple{right.at,source_order(right.source),right.source.revision,right.source.delivery};
            }),"unsorted principle transforms");
        for(const auto& transform:principle.transforms) {
            validate_source(transform.source,true);
            require(transform.at>=0&&transform.at<=now,"principle transform time outside history");
            require_unit(transform.target,"principle target outside [0,1]");
            require_unit(transform.alpha,"principle alpha outside [0,1]");
            require_unit(transform.quality,"principle quality outside [0,1]");
            require_unit(transform.dose,"principle dose outside [0,1]");
            require_unit(transform.plasticity,"principle plasticity outside [0,1]");
        }
        if(principle.has_explicit_prior||!principle.transforms.empty())
            require(!principle.checkpoints.empty(),"missing principle checkpoint");
    }
}

std::uint64_t NormMemory::record_revision(const NormKey& key_value) const {
    validate_key(key_value);
    auto position=record_position(records_,key_value);
    return position!=records_.end()&&position->key==key_value?position->revision:0;
}

std::vector<NormKey> NormMemory::keys(std::size_t offset,std::size_t limit) const {
    std::vector<NormKey> result;
    if(offset>=records_.size()||limit==0) return result;
    limit=std::min<std::size_t>(limit,profile_.candidate_ids_limit);
    const auto count=std::min(limit,records_.size()-offset);
    result.reserve(count);
    for(std::size_t i=0;i<count;++i) result.push_back(records_[offset+i].key);
    return result;
}

std::vector<NormKey> NormMemory::keys_for_context(std::uint32_t group,std::uint32_t context,
                                                  std::size_t limit) const {
    if(limit==0) return {};
    limit=std::min<std::size_t>(limit,profile_.candidate_ids_limit);
    auto cached=std::find_if(context_index_.begin(),context_index_.end(),
        [&](const auto& entry) {
            return entry.group==group&&entry.context==context&&entry.revision==revision_;
        });
    if(cached==context_index_.end()) {
        ContextIndexEntry entry;
        entry.group=group;
        entry.context=context;
        entry.revision=revision_;
        for(const auto& record:records_)
            if(record.key.local_group==group&&record.key.context==context)
                entry.keys.push_back(record.key);
        if(group!=0||context!=0)
            for(const auto& record:records_)
                if(record.key.local_group==0&&record.key.context==0)
                    entry.keys.push_back(record.key);
        context_index_.erase(std::remove_if(context_index_.begin(),context_index_.end(),
            [&](const auto& old){return old.group==group&&old.context==context;}),context_index_.end());
        context_index_.push_back(std::move(entry));
        cached=std::prev(context_index_.end());
    }
    const auto count=std::min(limit,cached->keys.size());
    return {cached->keys.begin(),cached->keys.begin()+static_cast<std::ptrdiff_t>(count)};
}

} // namespace life
