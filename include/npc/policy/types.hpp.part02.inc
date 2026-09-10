        f("source_ref", source_ref);
        f("received_at", received_at);
    }
};

struct KnownCommitment {
    Id id = {};
    Id proposal = {};
    std::int64_t version = 1;
    Id root = {};
    Id debtor = {};
    Id creditor = {};
    std::string kind = {};
    std::string status = "agreed";
    Json terms = Object{};
    Minute due = 0;
    bool important = false;
    Money amount = 0;
    std::optional<Money> remaining = {};
    std::optional<Minute> resolved_at = {};
    std::vector<Id> source_refs = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("proposal", proposal);
        f("version", version);
        f("root", root);
        f("debtor", debtor);
        f("creditor", creditor);
        f("kind", kind);
        f("status", status);
        f("terms", terms);
        f("due", due);
        f("important", important);
        f("amount", amount);
        f("remaining", remaining);
        f("resolved_at", resolved_at);
        f("source_refs", source_refs);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("proposal", proposal);
        f("version", version);
        f("root", root);
        f("debtor", debtor);
        f("creditor", creditor);
        f("kind", kind);
        f("status", status);
        f("terms", terms);
        f("due", due);
        f("important", important);
        f("amount", amount);
        f("remaining", remaining);
        f("resolved_at", resolved_at);
        f("source_refs", source_refs);
    }
};

struct PolicyState {
    std::string policy_version = "POLICY-0.1";
    std::string policy_config_hash = {};
    std::string world_config_hash = {};
    Id actor = {};
    std::uint64_t decision_sequence = 0;
    std::map<Id, std::int64_t> goal_episode_counters = {};
    std::vector<Goal> goals = {};
    std::optional<Plan> active_plan = {};
    std::uint64_t plan_cursor = 0;
    std::vector<SessionRuntime> session_runtimes = {};
    std::vector<CalendarIntent> calendar_intents = {};
    std::map<Id, Minute> cooldowns = {};
    std::vector<OutcomeHistoryEntry> outcome_history = {};
    ActorViewCursor knowledge_cursor = {};
    Minute next_regular_review_tick = 0;
    std::map<Id, std::uint64_t> fair_scan_cursor = {};
    std::vector<PendingCommandLink> pending_command_links = {};
    std::vector<PendingNotification> pending_notifications = {};
    std::optional<OwnState> last_own_state = {};
    template<class F> void visit(F&& f) {
        f("policy_version", policy_version);
        f("policy_config_hash", policy_config_hash);
        f("world_config_hash", world_config_hash);
        f("actor", actor);
        f("decision_sequence", decision_sequence);
        f("goal_episode_counters", goal_episode_counters);
        f("goals", goals);
        f("active_plan", active_plan);
        f("plan_cursor", plan_cursor);
        f("session_runtimes", session_runtimes);
        f("calendar_intents", calendar_intents);
        f("cooldowns", cooldowns);
        f("outcome_history", outcome_history);
        f("knowledge_cursor", knowledge_cursor);
        f("next_regular_review_tick", next_regular_review_tick);
        f("fair_scan_cursor", fair_scan_cursor);
        f("pending_command_links", pending_command_links);
        f("pending_notifications", pending_notifications);
        f("last_own_state", last_own_state);
    }
    template<class F> void visit(F&& f) const {
        f("policy_version", policy_version);
        f("policy_config_hash", policy_config_hash);
        f("world_config_hash", world_config_hash);
        f("actor", actor);
        f("decision_sequence", decision_sequence);
        f("goal_episode_counters", goal_episode_counters);
        f("goals", goals);
        f("active_plan", active_plan);
        f("plan_cursor", plan_cursor);
        f("session_runtimes", session_runtimes);
        f("calendar_intents", calendar_intents);
        f("cooldowns", cooldowns);
