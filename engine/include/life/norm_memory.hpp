#pragma once

#include "life/norm_types.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace life {

struct NormEvidenceContribution {
    NormChannel channel{};
    NormSource source{};
    Tick original_at{}, summary_at{};
    double positive{}, negative{};
    double severity_positive{}, severity_negative{};
    bool severity_known{};
    std::array<double,3> approval{};
    double dose{}, value{}, quality{}, confidence{}, reliability{1};
    ApprovalValue approval_value{};
    bool pooled{};
    template<class A> void fields(A& a) {
        a(channel,source,original_at,summary_at,positive,negative,severity_positive,
          severity_negative,severity_known,approval,dose,value,quality,confidence,
          reliability,approval_value,pooled);
    }
};

struct PersonalPrincipleTransform {
    NormSource source{};
    Tick at{};
    double target{}, alpha{}, quality{}, dose{}, plasticity{};
    template<class A> void fields(A& a) { a(source,at,target,alpha,quality,dose,plasticity); }
};

struct PersonalPrincipleCheckpoint {
    std::uint32_t transform_count{};
    double weight{};
    template<class A> void fields(A& a) { a(transform_count,weight); }
};

struct PersonalPrinciple {
    bool has_explicit_prior{};
    double prior_weight{};
    NormSource prior_source{};
    Tick prior_at{};
    std::uint32_t aspect{};
    double applicability{1}, exception{}, severity{1}, weight{};
    NormSource qualification_source{};
    Tick qualification_at{};
    std::vector<PersonalPrincipleTransform> transforms;
    std::vector<PersonalPrincipleCheckpoint> checkpoints;
    std::uint64_t revision{};
    template<class A> void fields(A& a) {
        a(has_explicit_prior,prior_weight,prior_source,prior_at,aspect,applicability,
          exception,severity,weight,qualification_source,qualification_at,
          transforms,checkpoints,revision);
    }
};

struct NormRecord {
    NormKey key{};
    BinaryNormEstimate descriptive{}, detection{}, classification{}, reaction{}, severity{};
    ApprovalEstimate approval{};
    PersonalPrinciple personal{};
    std::array<NormOrigin,6> origins{};
    std::array<bool,6> origin_known{};
    std::array<NormSource,6> prior_sources{};
    std::array<Tick,6> prior_at{};
    std::array<bool,6> prior_source_known{};
    NormSource severity_prior_source{};
    Tick severity_prior_at{};
    bool severity_prior_source_known{};
    std::vector<NormEvidenceContribution> sources;
    Tick summary_at{};
    std::uint64_t revision{};
    template<class A> void fields(A& a) {
        a(key,descriptive,detection,classification,reaction,severity,approval,personal,
          origins,origin_known,prior_sources,prior_at,prior_source_known,
          severity_prior_source,severity_prior_at,severity_prior_source_known,
          sources,summary_at,revision);
    }
};

struct NormExposureDose {
    Id observed_actor{};
    std::uint32_t practice{}, context{};
    std::int64_t day{};
    double dose{};
    auto operator<=>(const NormExposureDose&) const = default;
    template<class A> void fields(A& a) { a(observed_actor,practice,context,day,dose); }
};

class NormMemory {
public:
    NormMemory();
    explicit NormMemory(NormProfile profile);

    void configure(const NormProfile& profile);
    const NormProfile& profile() const { return profile_; }

    ApplyEvidence apply(const NormObservation& observation,Tick now);
    ApplyEvidence seed_binary(const NormKey& key,NormChannel channel,double probability,
                              double strength,const NormSource& source,Tick at);
    ApplyEvidence seed_approval(const NormKey& key,const std::array<double,3>& probabilities,
                                double strength,const NormSource& source,Tick at);
    ApplyEvidence seed_severity(const NormKey& key,double severity,double strength,
                                const NormSource& source,Tick at);
    ApplyEvidence seed_principle(const NormKey& key,double value,const NormSource& source,Tick at);
    ApplyEvidence qualify_principle(const NormKey& key,double applicability,double exception,
                                    double severity,std::uint32_t aspect,
                                    const NormSource& source,Tick at);
    NormPrediction predict(const NormKey& key,Tick now) const;
    std::optional<NormObservation> recall_evidence(const NormKey& key,NormChannel channel,Tick now) const;
    double personal_resistance(std::span<const NormKey> keys,Tick now) const;

    void validate(Tick now) const;
    std::uint64_t revision() const { return revision_; }
    std::uint64_t record_revision(const NormKey& key) const;
    std::size_t size() const { return records_.size(); }
    const std::vector<NormRecord>& records() const { return records_; }
    std::vector<NormKey> keys(std::size_t offset=0,std::size_t limit=32) const;
    std::vector<NormKey> keys_for_context(std::uint32_t group,std::uint32_t context,
                                          std::size_t limit=32) const;
    std::size_t hot_cache_size() const { return prediction_cache_.size(); }

    template<class A> void fields(A& a) { a(profile_,records_,deliveries_,exposures_,revision_); }

private:
    struct PredictionCacheEntry {
        NormKey key{};
        Tick at{};
        std::uint64_t record_revision{};
        NormPrediction prediction{};
    };
    struct ContextIndexEntry {
        std::uint32_t group{},context{};
        std::uint64_t revision{};
        std::vector<NormKey> keys;
    };

    NormProfile profile_{};
    std::vector<NormRecord> records_;
    std::vector<std::uint64_t> deliveries_;
    std::vector<NormExposureDose> exposures_;
    std::uint64_t revision_{};
    mutable std::vector<PredictionCacheEntry> prediction_cache_;
    mutable std::vector<ContextIndexEntry> context_index_;
};

} // namespace life
