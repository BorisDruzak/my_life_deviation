        f("outcome_history", outcome_history);
        f("knowledge_cursor", knowledge_cursor);
        f("next_regular_review_tick", next_regular_review_tick);
        f("fair_scan_cursor", fair_scan_cursor);
        f("pending_command_links", pending_command_links);
        f("pending_notifications", pending_notifications);
        f("last_own_state", last_own_state);
    }
};

namespace detail { struct LocalData; }
class LocalView;

class ControllerMemory {
public:
    ControllerMemory(Id actor, std::string world_config_hash, std::string policy_config_hash);
    const PolicyState& state() const noexcept { return state_; }
    PolicyState& edit_state() noexcept { return state_; }
    const ActorViewCursor& cursor() const noexcept { return state_.knowledge_cursor; }
    Json snapshot() const;
    static ControllerMemory restore(const Json& snapshot, const std::string& world_config_hash,
                                    const std::string& policy_config_hash);
private:
    PolicyState state_;
    std::shared_ptr<const detail::LocalData> data_;
    friend LocalView apply_delta(ControllerMemory&, const ActorViewDelta&);
    friend class LocalView;
};

class LocalView {
public:
    const OwnState& own() const noexcept { return own_; }
    const ActorViewCursor& cursor() const noexcept { return cursor_; }
    bool history_complete() const noexcept { return history_complete_; }
    std::size_t evidence_count() const noexcept;
    const Evidence* evidence(const Id& id) const noexcept;
    const LocalReceipt* receipt(const Id& id) const noexcept;
    const LocalEnvelope* message_envelope(const Id& id) const noexcept;
    std::vector<LocalEnvelope> unread_messages(std::size_t limit = 8) const;
    const KnownCommitment* commitment(const Id& proposal, std::int64_t version) const;
    std::vector<KnownCommitment> nearest_commitments(std::size_t limit = 8) const;
    // Explicit export/debug operation; never an implicit input stage for bounded search.
    Json export_json() const;
private:
    LocalView() = default;
    OwnState own_;
    ActorViewCursor cursor_;
    bool history_complete_ = false;
    std::shared_ptr<const detail::LocalData> data_;
    friend LocalView apply_delta(ControllerMemory&, const ActorViewDelta&);
    friend struct KnowledgeQueryAccess;
};

LocalView apply_delta(ControllerMemory& memory, const ActorViewDelta& delta);
std::string goal_key(const Goal& goal);
std::string commitment_key(const Id& proposal, std::int64_t version);
void validate_policy_state(const PolicyState& state);
} // namespace npc::policy
