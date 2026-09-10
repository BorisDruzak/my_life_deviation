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
