#pragma once
#include "npc/policy/types.hpp"
#include "persistent_map.hpp"

namespace npc::policy::detail {
struct ReceiptEffect {
    Money repaid = 0;
    bool returned = false;
    bool loan_started = false;
    std::string session_status;
    Id terminal_receipt;
    Minute tick = 0;
    std::vector<Id> source_refs;
};
struct ObservedObligation {
    std::optional<Json> creation;
    std::optional<Minute> fulfilled_at;
    std::vector<Id> source_refs;
};
struct LocalData {
    Id actor;
    std::string world_config_hash, policy_config_hash;
    std::string history_hash, inbox_hash;
    PersistentMap<LocalEnvelope> envelopes;
    PersistentMap<Id> envelope_order;
    PersistentMap<Id> envelope_ordinals;
    PersistentMap<Id> unread;
    PersistentMap<bool> read_messages;
    PersistentMap<Evidence> evidence;
    PersistentMap<Id> evidence_order;
    PersistentMap<PersistentMap<Id>> query_index;
    PersistentMap<LocalReceipt> receipts;
    PersistentMap<KnownCommitment> commitments;
    PersistentMap<Id> commitment_ids;
    PersistentMap<Id> deadlines;
    PersistentMap<ReceiptEffect> receipt_effects;
    PersistentMap<ObservedObligation> obligation_observations;
};
std::string ordinal_key(std::uint64_t value);
std::string claim_key(const Id& subject, const std::string& predicate, const Json& arguments);
void validate_evidence(const Evidence& evidence, Minute tick);
void validate_own_state(const OwnState& own);
void validate_receipt(const LocalReceipt& receipt, const Id& actor, Minute tick);
void index_evidence(LocalData& data, const Evidence& evidence, std::uint64_t ordinal);
void index_envelope(LocalData& data, LocalEnvelope envelope, std::uint64_t ordinal, Minute tick);
void mark_message_read(LocalData& data, const Id& message);
void ingest_commitment_evidence(LocalData& data, const Evidence& evidence, const Id& actor);
void ingest_receipt(LocalData& data, const LocalReceipt& receipt, const Id& actor);
Json export_data(const LocalData& data);
LocalData restore_data(const Json& data, const PolicyState& state);
}
