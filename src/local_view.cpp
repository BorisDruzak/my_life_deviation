#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>

namespace npc {
std::string local_history_seed(const Id& actor, const std::string& config_hash) {
    return math::sha256(canonical(Array{"ACTOR-HISTORY-1", actor, config_hash}));
}
std::string local_history_next(const std::string& previous, const Evidence& evidence) {
    return math::sha256(previous + "\n" + canonical(encode(evidence)));
}
std::string local_inbox_seed(const Id& actor, const std::string& config_hash) {
    return math::sha256(canonical(Array{"ACTOR-INBOX-1",actor,config_hash}));
}
std::string local_inbox_next(const std::string& previous, const LocalEnvelope& envelope) {
    // Read status is mutable. The chain identifies delivered envelopes, never hidden bodies.
    return math::sha256(previous + "\n" + canonical(Array{envelope.id,envelope.from,envelope.channel,envelope.delivered_at}));
}
void World::rebuild_local_evidence_index() {
    local_evidence_prefixes_.clear();
    local_inbox_prefixes_.clear();
    for (const auto& [id, actor] : state_.actors) {
        auto& prefixes = local_evidence_prefixes_[id];
        prefixes.reserve(actor.beliefs.size() + 1);
        prefixes.push_back(local_history_seed(id, config_.hash));
        for (const auto& e : actor.beliefs) prefixes.push_back(local_history_next(prefixes.back(), e));
        auto& inbox=local_inbox_prefixes_[id];
        inbox.reserve(actor.inbox.size()+1); inbox.push_back(local_inbox_seed(id,config_.hash));
        for (const auto& mid : actor.inbox) {
            const auto& m=state_.messages.at(mid);
            inbox.push_back(local_inbox_next(inbox.back(),{mid,m.author,m.channel,m.deliver_at,false}));
        }
    }
}
ActorViewDelta World::view_delta(const Id& id, const ActorViewCursor& cursor, std::size_t limit,
                                const std::vector<Id>& receipt_ids) const {
    if (phase_ != "ready" || !state_.actors.contains(id)) throw InputError("invalid local view boundary/actor");
    if (limit < 1 || limit > local_page_limit || receipt_ids.size() > local_receipt_limit)
        throw InputError("local view budget out of range");
    const auto& actor = state_.actors.at(id);
    const auto& prefixes = local_evidence_prefixes_.at(id);
    const auto& inbox_prefixes=local_inbox_prefixes_.at(id);
    if (cursor.actor != id || cursor.offset > actor.beliefs.size() || cursor.inbox_offset > actor.inbox.size() ||
        cursor.prefix_hash != prefixes.at(static_cast<std::size_t>(cursor.offset)) ||
        cursor.inbox_prefix_hash != inbox_prefixes.at(static_cast<std::size_t>(cursor.inbox_offset)))
        throw InputError("foreign, stale or invalid local history cursor");
    ActorViewDelta out;
    out.world_config_hash = config_.hash;
    out.from = cursor;
    out.next = cursor;
    out.own.actor = id; out.own.tick = state_.time; out.own.profile = state_.profile;
    out.own.position = actor.position;
    // The body snapshot contains the registered needs, not arbitrary maps/history from Actor.
    for (const auto& entry : at(config_.parameters, "needs").as_object()) {
        const std::string key(entry.key());
        if (auto it = actor.needs.find(key); it != actor.needs.end()) out.own.needs.emplace(key, it->second);
    }
    out.own.money = state_.accounts.at(actor.account);
    out.own.health = actor.health; out.own.fatigue = actor.fatigue; out.own.pain = actor.pain;
    out.own.impairment = actor.impairment; out.own.stress = actor.stress; out.own.fear = actor.fear;
    out.own.selfesteem = actor.selfesteem; out.own.wellbeing = actor.wellbeing;
    out.own.capacity_kg = actor.capacity_kg; out.own.base_speed = actor.base_speed;
    out.own.attention = actor.attention; out.own.alive = actor.alive; out.own.capable = actor.capable;
    out.own.heavy_allowed = actor.heavy_allowed; out.own.active_action = actor.active_action;
    out.own.horizon = actor.horizon; out.own.search_width = actor.search_width;
    // Both append-only streams share one budget. Alternate when both have pending records.
    // This prevents a large evidence backlog from starving new unread-envelope notifications.
    while (out.records_examined < limit &&
           (out.next.offset < actor.beliefs.size() || out.next.inbox_offset < actor.inbox.size())) {
        const bool take_inbox=out.next.inbox_offset < actor.inbox.size() &&
                              (out.next.inbox_next || out.next.offset >= actor.beliefs.size());
        if (take_inbox) {
            const auto& mid=actor.inbox[static_cast<std::size_t>(out.next.inbox_offset++)];
            const auto& m=state_.messages.at(mid);
            out.inbox.push_back({mid,m.author,m.channel,m.deliver_at,m.readers.contains(id)});
        } else out.evidence.push_back(actor.beliefs[static_cast<std::size_t>(out.next.offset++)]);
        out.next.inbox_next=!take_inbox;
        ++out.records_examined;
    }
    out.next.prefix_hash=prefixes.at(static_cast<std::size_t>(out.next.offset));
    out.next.inbox_prefix_hash=inbox_prefixes.at(static_cast<std::size_t>(out.next.inbox_offset));
    out.has_more=out.next.offset < actor.beliefs.size() || out.next.inbox_offset < actor.inbox.size();
    std::set<Id> unique;
    for (const auto& key : receipt_ids) {
        if (!unique.insert(key).second) throw InputError("duplicate receipt watch");
        // Both absent and foreign IDs are unavailable. Do not disclose their existence or reason.
        const auto it = state_.receipts.find(key);
        if (it == state_.receipts.end() || it->second.actor != id) continue;
        const auto& r = it->second;
        out.receipts.push_back({key, id, r.action, r.status, r.public_reason, r.received, r.request, state_.time});
    }
    return out;
}
} // namespace npc
