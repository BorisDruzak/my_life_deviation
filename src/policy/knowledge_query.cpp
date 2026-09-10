#include "npc/policy/knowledge_query.hpp"
#include "types_internal.hpp"
#include "npc/math.hpp"
#include <algorithm>

namespace npc::policy {
struct KnowledgeQueryAccess {
    static const detail::LocalData& data(const LocalView& view) { return *view.data_; }
};
std::string to_string(Truth truth) {
    switch (truth) {
    case Truth::KnownTrue: return "KnownTrue";
    case Truth::KnownFalse: return "KnownFalse";
    case Truth::Unknown: return "Unknown";
    }
    throw InputError("invalid belief truth enum");
}
BeliefResult query_belief(const LocalView& view,const BeliefQuery& q,
                         const std::optional<PersistenceRule>& persistence,std::size_t limit) {
    if (q.subject.empty() || q.predicate.empty() || q.at < 0 || q.at > view.own().tick ||
        limit < 1 || limit > local_page_limit) throw InputError("invalid local belief query/time/budget");
    if (persistence && (persistence->predicate != q.predicate || !std::isfinite(persistence->half_life_minutes) ||
        persistence->half_life_minutes <= 0 || persistence->half_life_minutes >= static_cast<double>(max_minute) ||
        persistence->prior != .5)) throw InputError("invalid persistence rule; prior must be neutral");
    BeliefResult out;
    const auto& data=KnowledgeQueryAccess::data(view);
    const auto* bucket=data.query_index.find(detail::claim_key(q.subject,q.predicate,q.arguments));
    double yes=.5, no=.5;
    bool expired=false, future=false, assumed=false;
    if (bucket) bucket->visit(limit,[&](const Id&,const Id& id) {
        const auto& e=*data.evidence.find(id); ++out.examined;
        const auto& s=e.statement;
        if (e.learned_at > view.own().tick || q.at < s.valid_from) { future=true; return; }
        Minute until=s.valid_until;
        const bool snapshot_predicate = s.predicate == "item_seen" || s.predicate == "shop_stock" ||
                                        s.predicate == "person_seen";
        if (snapshot_predicate && e.kind != "hypothesis") until=s.valid_from;
        double confidence=e.confidence;
        bool inferred=false;
        Minute observed=s.valid_from;
        if (until >= 0 && q.at > until) {
            expired=true;
            if (!persistence || e.kind != "observation") return;
            observed=until;
            confidence=math::stale(e.confidence,persistence->prior,static_cast<double>(q.at-until),persistence->half_life_minutes);
            inferred=true;
        } else if (e.kind == "hypothesis" && e.half_life > 0) {
            confidence=math::stale(e.confidence,e.prior,static_cast<double>(std::max<Minute>(0,q.at-e.learned_at)),e.half_life);
            inferred=true;
        }
        if (confidence <= .5) return;
        if (s.polarity == q.polarity) yes=std::max(yes,confidence);
        else no=std::max(no,confidence);
        out.sources.push_back({e.id,e.source,e.roots,confidence,inferred,observed,q.at});
        assumed=assumed || inferred;
    },true);
    std::sort(out.sources.begin(),out.sources.end(),[](const EvidenceUse& a,const EvidenceUse& b){return a.id<b.id;});
    if (!view.history_complete()) {
        out.complete=false; out.reason="unprocessed_local_history"; return out;
    }
    if (bucket && bucket->size() > limit) {
        out.complete=false; out.reason="query_budget_exhausted"; return out;
    }
    if (yes>.5 && no>.5) { out.reason="conflicting_sources"; return out; }
    if (yes>.5) out.confidence=yes;
    else if (no>.5) out.confidence=1-no;
    if (yes>=.75) out.truth=Truth::KnownTrue;
    else if (no>=.75) out.truth=Truth::KnownFalse;
    if (out.truth != Truth::Unknown) out.reason=assumed ? "persistence_assumption" : "explicit_temporal_evidence";
    else if (yes>.5 || no>.5) out.reason="insufficient_confidence";
    else if (expired) out.reason="expired_evidence";
    else if (future) out.reason="outside_valid_interval";
    return out;
}
Json describe(const BeliefResult& r) {
    Array sources;
    for (const auto& s : r.sources) sources.push_back(Object{{"id",s.id},{"source",s.source},{"roots",encode(s.roots)},
        {"confidence",s.confidence},{"assumed",s.assumed},{"observed_at",s.observed_at},{"applies_at",s.applies_at}});
    return Object{{"truth",to_string(r.truth)},{"confidence",r.confidence},{"reason",r.reason},
                  {"sources",std::move(sources)},{"examined",r.examined},{"complete",r.complete}};
}
} // namespace npc::policy
