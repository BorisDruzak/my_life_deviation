#pragma once
#include "npc/local_view.hpp"
#include <memory>

namespace npc::policy {
// T1 defines state contracts only. Goal generation and search are separate stages.
struct GoalTarget {
    std::string kind = {};
    Id id = {};
    std::int64_t version = 0;
    Json specification = Object{};
    template<class F> void visit(F&& f) {
        f("kind", kind);
        f("id", id);
        f("version", version);
        f("specification", specification);
    }
    template<class F> void visit(F&& f) const {
        f("kind", kind);
        f("id", id);
        f("version", version);
        f("specification", specification);
    }
};

struct Goal {
    Id id = {};
    Id actor = {};
    std::string family = "Need";
    GoalTarget target = {};
    Id source_root = {};
    std::vector<Id> source_refs = {};
    std::vector<Id> parent_goal_ids = {};
    std::int64_t episode = 1;
    Minute created_at = 0;
    std::string activation_reason = {};
    Json desired_predicate = Object{};
    Minute earliest_start = 0;
    std::optional<Minute> deadline = {};
    std::string status = "candidate";
    std::string blocked_reason = {};
    std::string wake_trigger = {};
    std::optional<Minute> next_review_at = {};
    double urgency = 0;
    double importance = 1;
    Minute last_progress_at = 0;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("actor", actor);
        f("family", family);
        f("target", target);
        f("source_root", source_root);
        f("source_refs", source_refs);
        f("parent_goal_ids", parent_goal_ids);
        f("episode", episode);
        f("created_at", created_at);
        f("activation_reason", activation_reason);
        f("desired_predicate", desired_predicate);
        f("earliest_start", earliest_start);
        f("deadline", deadline);
        f("status", status);
        f("blocked_reason", blocked_reason);
        f("wake_trigger", wake_trigger);
        f("next_review_at", next_review_at);
        f("urgency", urgency);
        f("importance", importance);
        f("last_progress_at", last_progress_at);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("actor", actor);
        f("family", family);
        f("target", target);
        f("source_root", source_root);
        f("source_refs", source_refs);
        f("parent_goal_ids", parent_goal_ids);
        f("episode", episode);
        f("created_at", created_at);
        f("activation_reason", activation_reason);
        f("desired_predicate", desired_predicate);
        f("earliest_start", earliest_start);
        f("deadline", deadline);
        f("status", status);
        f("blocked_reason", blocked_reason);
        f("wake_trigger", wake_trigger);
        f("next_review_at", next_review_at);
        f("urgency", urgency);
        f("importance", importance);
        f("last_progress_at", last_progress_at);
    }
};

struct PlanStep {
    Id id = {};
    Id method = {};
    Command command = {};
    Minute duration = 1;
    bool observation_barrier = false;
    std::vector<Id> source_refs = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("method", method);
        f("command", command);
        f("duration", duration);
        f("observation_barrier", observation_barrier);
        f("source_refs", source_refs);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("method", method);
        f("command", command);
        f("duration", duration);
        f("observation_barrier", observation_barrier);
        f("source_refs", source_refs);
    }
};

struct Plan {
    Id id = {};
    std::vector<Id> goal_ids = {};
    Minute created_at = 0;
    std::string status = "candidate";
    std::vector<PlanStep> steps = {};
    std::vector<Id> source_refs = {};
    Json assumptions = Array{};
    std::optional<double> score = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("goal_ids", goal_ids);
        f("created_at", created_at);
        f("status", status);
        f("steps", steps);
        f("source_refs", source_refs);
        f("assumptions", assumptions);
        f("score", score);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("goal_ids", goal_ids);
        f("created_at", created_at);
        f("status", status);
        f("steps", steps);
        f("source_refs", source_refs);
        f("assumptions", assumptions);
        f("score", score);
    }
};

struct SessionRuntime {
    Id id = {};
    Id proposal = {};
    std::int64_t version = 1;
    std::string status = "created";
    Minute updated_at = 0;
    std::optional<Minute> first_wait = {};
    Minute effect_minutes = 0;
    std::vector<Id> source_refs = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("proposal", proposal);
        f("version", version);
        f("status", status);
        f("updated_at", updated_at);
        f("first_wait", first_wait);
        f("effect_minutes", effect_minutes);
        f("source_refs", source_refs);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("proposal", proposal);
        f("version", version);
        f("status", status);
        f("updated_at", updated_at);
        f("first_wait", first_wait);
        f("effect_minutes", effect_minutes);
        f("source_refs", source_refs);
    }
};

struct CalendarIntent {
    Id id = {};
    Id actor = {};
    Id goal_id = {};
    Id session_id = {};
    Minute start = 0;
    Minute end = 0;
    Minute travel_buffer = 0;
    std::string status = "tentative";
    template<class F> void visit(F&& f) {
        f("id", id);
        f("actor", actor);
        f("goal_id", goal_id);
        f("session_id", session_id);
        f("start", start);
        f("end", end);
        f("travel_buffer", travel_buffer);
        f("status", status);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("actor", actor);
        f("goal_id", goal_id);
        f("session_id", session_id);
        f("start", start);
        f("end", end);
        f("travel_buffer", travel_buffer);
        f("status", status);
    }
};

struct OutcomeHistoryEntry {
    Id root = {};
    Id partner = {};
    std::string activity_family = {};
    std::string outcome = "unknown";
    Minute learned_at = 0;
    Id source_ref = {};
    template<class F> void visit(F&& f) {
        f("root", root);
        f("partner", partner);
        f("activity_family", activity_family);
        f("outcome", outcome);
        f("learned_at", learned_at);
        f("source_ref", source_ref);
    }
    template<class F> void visit(F&& f) const {
        f("root", root);
        f("partner", partner);
        f("activity_family", activity_family);
        f("outcome", outcome);
        f("learned_at", learned_at);
        f("source_ref", source_ref);
    }
};

struct PendingCommandLink {
    Id command_id = {};
    Id goal_id = {};
    Id plan_id = {};
    Id session_id = {};
    Minute submitted_at = 0;
    template<class F> void visit(F&& f) {
        f("command_id", command_id);
        f("goal_id", goal_id);
        f("plan_id", plan_id);
        f("session_id", session_id);
        f("submitted_at", submitted_at);
    }
    template<class F> void visit(F&& f) const {
        f("command_id", command_id);
        f("goal_id", goal_id);
        f("plan_id", plan_id);
        f("session_id", session_id);
        f("submitted_at", submitted_at);
    }
};

struct PendingNotification {
    Id id = {};
    std::string kind = {};
    Id source_ref = {};
    Minute received_at = 0;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("kind", kind);
        f("source_ref", source_ref);
        f("received_at", received_at);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("kind", kind);
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
