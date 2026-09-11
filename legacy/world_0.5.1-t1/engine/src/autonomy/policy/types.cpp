#include "types_internal.hpp"
#include "npc/math.hpp"
#include <algorithm>
#include <limits>

namespace npc::policy {
namespace detail {
std::string ordinal_key(std::uint64_t value) {
    auto key = std::to_string(value);
    return std::string(20 - key.size(), '0') + key;
}
std::string claim_key(const Id& subject, const std::string& predicate, const Json& arguments) {
    return canonical(Array{subject, predicate, arguments});
}
void index_evidence(LocalData& data, const Evidence& e, std::uint64_t ordinal) {
    if (data.evidence.find(e.id)) throw InputError("duplicate indexed evidence id");
    const auto order = ordinal_key(ordinal);
    const auto key = claim_key(e.statement.subject,e.statement.predicate,e.statement.arguments);
    auto bucket = data.query_index.find(key) ? *data.query_index.find(key) : PersistentMap<Id>{};
    bucket.set(order,e.id);
    data.query_index.set(key,std::move(bucket));
    data.evidence.set(e.id,e);
    data.evidence_order.set(order,e.id);
    data.history_hash=local_history_next(data.history_hash,e);
    const auto suffix="/read/"+data.actor;
    if (e.kind=="claim" && e.id.size()>suffix.size() && e.id.ends_with(suffix))
        mark_message_read(data,e.id.substr(0,e.id.size()-suffix.size()));
}
void mark_message_read(LocalData& data,const Id& message) {
    data.read_messages.set(message,true);
    if (const auto* old=data.envelopes.find(message)) {
        auto envelope=*old; envelope.read=true; data.envelopes.set(message,std::move(envelope));
        data.unread.erase(*data.envelope_ordinals.find(message));
    }
}
void index_envelope(LocalData& data,LocalEnvelope envelope,std::uint64_t ordinal,Minute tick) {
    if (envelope.id.empty() || envelope.from.empty() || envelope.id.size()>1024 || envelope.from.size()>256 ||
        (envelope.channel!="speech" && envelope.channel!="digital") || envelope.delivered_at<0 || envelope.delivered_at>tick)
        throw InputError("invalid local envelope");
    if (data.envelopes.find(envelope.id)) throw InputError("duplicate local envelope");
    const auto order=ordinal_key(ordinal);
    data.inbox_hash=local_inbox_next(data.inbox_hash,envelope);
    if (data.read_messages.find(envelope.id)) envelope.read=true;
    if (envelope.read) data.read_messages.set(envelope.id,true);
    else data.unread.set(order,envelope.id);
    data.envelope_order.set(order,envelope.id); data.envelope_ordinals.set(envelope.id,order);
    const auto id=envelope.id; data.envelopes.set(id,std::move(envelope));
}
Json export_data(const LocalData& data) {
    Array evidence, commitments, receipts, inbox;
    data.evidence_order.visit(data.evidence_order.size(),[&](const Id&,const Id& id) {
        evidence.push_back(encode(*data.evidence.find(id)));
    });
    data.envelope_order.visit(data.envelope_order.size(),[&](const Id&,const Id& id) { inbox.push_back(encode(*data.envelopes.find(id))); });
    data.commitments.visit(data.commitments.size(),[&](const Id&,const KnownCommitment& c) { commitments.push_back(encode(c)); });
    data.receipts.visit(data.receipts.size(),[&](const Id&,const LocalReceipt& r) { receipts.push_back(encode(r)); });
    return Object{{"known_query_index",Object{{"version","LOCAL-KNOWLEDGE-1"},{"evidence",std::move(evidence)},{"rolling_hash",data.history_hash},{"inbox",std::move(inbox)},{"inbox_hash",data.inbox_hash}}},
                  {"known_commitments",std::move(commitments)},{"receipt_index",std::move(receipts)}};
}
LocalData restore_data(const Json& value, const PolicyState& state) {
    if (!value.is_object()) throw InputError("local data must be an object");
    const auto& knowledge = at(value,"known_query_index");
    if (str(knowledge,"version") != "LOCAL-KNOWLEDGE-1") throw InputError("unknown knowledge index version");
    const auto es = get<std::vector<Evidence>>(knowledge,"evidence");
    if (es.size() != state.knowledge_cursor.offset) throw InputError("knowledge cursor/count mismatch");
    LocalData data;
    data.actor=state.actor; data.world_config_hash=state.world_config_hash; data.policy_config_hash=state.policy_config_hash;
    data.history_hash=local_history_seed(state.actor,state.world_config_hash);
    data.inbox_hash=local_inbox_seed(state.actor,state.world_config_hash);
    const Minute now = state.last_own_state ? state.last_own_state->tick : 0;
    for (const auto& e : es) {
        validate_evidence(e,now);
        index_evidence(data,e,data.evidence.size());
        ingest_commitment_evidence(data,e,state.actor);
    }
    if (data.history_hash != state.knowledge_cursor.prefix_hash) throw InputError("knowledge rolling hash mismatch");
    const auto inbox=get<std::vector<LocalEnvelope>>(knowledge,"inbox");
    if (inbox.size()!=state.knowledge_cursor.inbox_offset) throw InputError("inbox cursor/count mismatch");
    for (const auto& envelope : inbox) index_envelope(data,envelope,data.envelopes.size(),now);
    if (data.inbox_hash!=state.knowledge_cursor.inbox_prefix_hash) throw InputError("inbox rolling hash mismatch");
    const auto rs = get<std::vector<LocalReceipt>>(value,"receipt_index");
    std::set<Id> ids;
    for (const auto& r : rs) {
        if (!ids.insert(r.id).second) throw InputError("duplicate saved receipt id");
        validate_receipt(r,state.actor,now);
        ingest_receipt(data,r,state.actor);
    }
    // Derived commitments and secondary indices cannot be forged independently of local sources.
    if (canonical(export_data(data)) != canonical(value))
        throw InputError("incomplete, noncanonical or inconsistent local knowledge/commitment index");
    return data;
}
} // namespace detail

std::string goal_key(const Goal& g) {
    return canonical(Array{g.actor,g.family,encode(g.target),g.source_root,g.episode});
}
std::string commitment_key(const Id& proposal, std::int64_t version) {
    return canonical(Array{proposal,version});
}
ControllerMemory::ControllerMemory(Id actor, std::string world_hash, std::string policy_hash)
    : data_(std::make_shared<const detail::LocalData>()) {
    state_.actor=std::move(actor); state_.world_config_hash=std::move(world_hash);
    state_.policy_config_hash=std::move(policy_hash);
    state_.knowledge_cursor.actor=state_.actor;
    state_.knowledge_cursor.prefix_hash=local_history_seed(state_.actor,state_.world_config_hash);
    state_.knowledge_cursor.inbox_prefix_hash=local_inbox_seed(state_.actor,state_.world_config_hash);
    validate_policy_state(state_);
    auto data=std::make_shared<detail::LocalData>();
    data->actor=state_.actor; data->world_config_hash=state_.world_config_hash; data->policy_config_hash=state_.policy_config_hash;
    data->history_hash=state_.knowledge_cursor.prefix_hash; data->inbox_hash=state_.knowledge_cursor.inbox_prefix_hash;
    data_=std::move(data);
}
Json ControllerMemory::snapshot() const {
    validate_policy_state(state_);
    if (state_.actor!=data_->actor || state_.world_config_hash!=data_->world_config_hash || state_.policy_config_hash!=data_->policy_config_hash)
        throw InputError("policy memory identity/configuration cannot be relabelled");
    if (state_.knowledge_cursor.offset!=data_->evidence.size() || state_.knowledge_cursor.prefix_hash!=data_->history_hash ||
        state_.knowledge_cursor.inbox_offset!=data_->envelopes.size() || state_.knowledge_cursor.inbox_prefix_hash!=data_->inbox_hash)
        throw InputError("edited knowledge/inbox cursor");
    auto payload=detail::export_data(*data_);
    payload.as_object()["control"]=encode(state_);
    return Object{{"format","npc-policy-memory"},{"version","POLICY-STATE-1"},
                  {"world_config_hash",state_.world_config_hash},{"policy_config_hash",state_.policy_config_hash},
                  {"state_hash",math::sha256(canonical(payload))},{"state",std::move(payload)}};
}
ControllerMemory ControllerMemory::restore(const Json& snapshot,const std::string& world_hash,const std::string& policy_hash) {
    if (str(snapshot,"format") != "npc-policy-memory" || str(snapshot,"version") != "POLICY-STATE-1" ||
        str(snapshot,"world_config_hash") != world_hash || str(snapshot,"policy_config_hash") != policy_hash)
        throw InputError("incompatible policy memory version/configuration");
    const auto& payload=at(snapshot,"state");
    if (math::sha256(canonical(payload)) != str(snapshot,"state_hash")) throw InputError("policy memory checksum mismatch");
    auto state=decode<PolicyState>(at(payload,"control"));
    validate_policy_state(state);
    if (state.world_config_hash != world_hash || state.policy_config_hash != policy_hash ||
        canonical(encode(state)) != canonical(at(payload,"control"))) throw InputError("incomplete/incompatible policy state");
    auto local=payload; local.as_object().erase("control");
    auto data=detail::restore_data(local,state);
    ControllerMemory memory(state.actor,world_hash,policy_hash);
    memory.state_=std::move(state);
    memory.data_=std::make_shared<const detail::LocalData>(std::move(data));
    if (canonical(memory.snapshot()) != canonical(snapshot)) throw InputError("noncanonical/incomplete policy snapshot");
    return memory;
}
LocalView apply_delta(ControllerMemory& memory,const ActorViewDelta& delta) {
    const auto& state=memory.state_;
    if (state.actor!=memory.data_->actor || state.world_config_hash!=memory.data_->world_config_hash || state.policy_config_hash!=memory.data_->policy_config_hash)
        throw InputError("policy memory identity/configuration cannot be relabelled");
    if (delta.version != "ACTOR-VIEW-1" || delta.world_config_hash != state.world_config_hash ||
        delta.own.actor != state.actor || delta.next.actor != state.actor ||
        canonical(encode(delta.from)) != canonical(encode(state.knowledge_cursor)))
        throw InputError("out-of-order/foreign local delta");
    if (delta.evidence.size()+delta.inbox.size() > local_page_limit || delta.receipts.size() > local_receipt_limit ||
        delta.records_examined != delta.evidence.size()+delta.inbox.size() || delta.from.offset != memory.data_->evidence.size() ||
        delta.from.prefix_hash != memory.data_->history_hash || delta.from.inbox_offset!=memory.data_->envelopes.size() ||
        delta.from.inbox_prefix_hash!=memory.data_->inbox_hash || delta.next.inbox_offset<delta.from.inbox_offset ||
        delta.next.inbox_offset-delta.from.inbox_offset!=delta.inbox.size() ||
        delta.next.offset < delta.from.offset || delta.next.offset-delta.from.offset != delta.evidence.size())
        throw InputError("invalid local delta budget/cursor");
    detail::validate_own_state(delta.own);
    if (state.last_own_state && delta.own.tick < state.last_own_state->tick) throw InputError("local time went backwards");
    // Copy persistent roots only. Validation/index updates are staged: any bad record leaves memory untouched.
    auto data=std::make_shared<detail::LocalData>(*memory.data_);
    for (const auto& e : delta.evidence) {
        detail::validate_evidence(e,delta.own.tick);
        detail::index_evidence(*data,e,data->evidence.size());
        detail::ingest_commitment_evidence(*data,e,state.actor);
    }
    for (const auto& envelope : delta.inbox) detail::index_envelope(*data,envelope,data->envelopes.size(),delta.own.tick);
    if (data->history_hash != delta.next.prefix_hash || data->inbox_hash!=delta.next.inbox_prefix_hash)
        throw InputError("local delta rolling hash mismatch");
    std::set<Id> seen;
    for (const auto& r : delta.receipts) {
        if (!seen.insert(r.id).second) throw InputError("duplicate delta receipt");
        detail::validate_receipt(r,state.actor,delta.own.tick);
        detail::ingest_receipt(*data,r,state.actor);
    }
    LocalView view;
    view.own_=delta.own; view.cursor_=delta.next; view.history_complete_=!delta.has_more;
    view.data_=data;
    memory.state_.knowledge_cursor=delta.next;
    memory.state_.last_own_state=delta.own;
    memory.data_=std::move(data);
    return view;
}
std::size_t LocalView::evidence_count() const noexcept { return data_->evidence.size(); }
const Evidence* LocalView::evidence(const Id& id) const noexcept { return data_->evidence.find(id); }
const LocalReceipt* LocalView::receipt(const Id& id) const noexcept { return data_->receipts.find(id); }
const LocalEnvelope* LocalView::message_envelope(const Id& id) const noexcept { return data_->envelopes.find(id); }
std::vector<LocalEnvelope> LocalView::unread_messages(std::size_t limit) const {
    if (limit<1 || limit>local_page_limit) throw InputError("unread envelope budget out of range");
    std::vector<LocalEnvelope> result;
    data_->unread.visit(limit,[&](const Id&,const Id& id){result.push_back(*data_->envelopes.find(id));});
    return result;
}
const KnownCommitment* LocalView::commitment(const Id& proposal,std::int64_t version) const {
    return data_->commitments.find(commitment_key(proposal,version));
}
std::vector<KnownCommitment> LocalView::nearest_commitments(std::size_t limit) const {
    if (limit < 1 || limit > local_page_limit) throw InputError("deadline page budget out of range");
    std::vector<KnownCommitment> result;
    data_->deadlines.visit(limit,[&](const Id&,const Id& key) { result.push_back(*data_->commitments.find(key)); });
    return result;
}
Json LocalView::export_json() const {
    auto result=detail::export_data(*data_);
    result.as_object()["own"]=encode(own_); result.as_object()["cursor"]=encode(cursor_);
    result.as_object()["history_complete"]=history_complete_;
    return result;
}
} // namespace npc::policy
