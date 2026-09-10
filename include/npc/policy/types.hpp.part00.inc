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
