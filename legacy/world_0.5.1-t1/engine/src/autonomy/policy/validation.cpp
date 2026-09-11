#include "types_internal.hpp"
#include <algorithm>
#include <cctype>
#include <functional>

namespace npc::policy {
namespace {
void need(bool ok, const std::string& message) { if (!ok) throw InputError(message); }
void tick(Minute value) { need(value >= 0 && value < max_minute, "negative or out-of-range policy tick"); }
void maybe_tick(const std::optional<Minute>& value) { if (value) tick(*value); }
void ident(const std::string& value) {
    need(!value.empty() && value.size() <= 1024, "empty/oversized policy identifier");
    for (unsigned char c : value) need(c >= 32 && c != 127, "control character in policy identifier");
}
void choice(const std::string& value, std::initializer_list<std::string_view> options, const char* name) {
    need(std::find(options.begin(), options.end(), value) != options.end(), std::string("unknown ") + name + ": " + value);
}
void ratio(double value, double upper = 1) { need(std::isfinite(value) && value >= 0 && value <= upper, "invalid policy number"); }
void finite(const Json& value, unsigned depth = 0) {
    need(depth <= 64, "policy JSON nesting limit exceeded");
    if (value.is_double()) need(std::isfinite(value.as_double()), "non-finite policy JSON number");
    if (value.is_object()) for (const auto& x : value.as_object()) finite(x.value(), depth + 1);
    if (value.is_array()) for (const auto& x : value.as_array()) finite(x, depth + 1);
}
void hash(const std::string& value) {
    need(value.size() == 64 && std::all_of(value.begin(),value.end(),[](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    }), "invalid policy SHA-256");
}
void ids(const std::vector<Id>& values) {
    std::set<Id> seen;
    for (const auto& id : values) { ident(id); need(seen.insert(id).second,"duplicate policy reference"); }
}
void actor_id(const Id& id) { ident(id); need(id.find_first_of("@/") == std::string::npos, "invalid policy actor"); }
}
void validate_policy_state(const PolicyState& s) {
    need(s.policy_version == "POLICY-0.1", "unknown policy version");
    actor_id(s.actor); hash(s.world_config_hash); hash(s.policy_config_hash);
    need(s.decision_sequence <= static_cast<std::uint64_t>(max_minute), "policy sequence overflow");
    need(s.knowledge_cursor.actor == s.actor, "foreign knowledge cursor"); hash(s.knowledge_cursor.prefix_hash); hash(s.knowledge_cursor.inbox_prefix_hash);
    tick(s.next_regular_review_tick);
    for (const auto& [key, count] : s.goal_episode_counters) {
        ident(key); need(count >= 0 && count < max_minute, "invalid goal episode counter");
    }
    std::set<Id> goal_ids, goal_keys;
    for (const auto& g : s.goals) {
        ident(g.id); need(g.actor == s.actor, "foreign goal actor"); ident(g.source_root);
        need(goal_ids.insert(g.id).second, "duplicate goal id");
        need(goal_keys.insert(goal_key(g)).second, "duplicate canonical goal key");
        choice(g.family, {"Need","MaintainCondition","FulfillCommitment","AcquireResource","EarnBudget",
                          "PersonalProject","ResolveUncertainty","RespondProposal","AttendSession"}, "goal family");
        choice(g.target.kind, {"need","condition","obligation","resource","budget","project","query","proposal","session"}, "goal target kind");
        const std::map<std::string,std::string> targets={{"Need","need"},{"MaintainCondition","condition"},{"FulfillCommitment","obligation"},
            {"AcquireResource","resource"},{"EarnBudget","budget"},{"PersonalProject","project"},{"ResolveUncertainty","query"},
            {"RespondProposal","proposal"},{"AttendSession","session"}};
        need(targets.at(g.family)==g.target.kind,"goal family/target mismatch");
        if (g.target.kind=="proposal" || g.target.kind=="session") need(g.target.version>=1,"unversioned proposal/session target");
        ident(g.target.id); need(g.target.version >= 0 && g.target.version < max_minute, "invalid goal target version");
        finite(g.target.specification); finite(g.desired_predicate);
        need(g.episode >= 1 && g.episode < max_minute, "invalid goal episode");
        ids(g.source_refs); ids(g.parent_goal_ids);
        tick(g.created_at); tick(g.earliest_start); tick(g.last_progress_at);
        maybe_tick(g.deadline); maybe_tick(g.next_review_at);
        need(g.last_progress_at >= g.created_at, "goal progress predates creation");
        choice(g.status,{"candidate","active","satisfied","waiting_external","blocked","suspended","cancelled","expired","failed"},"goal status");
        if (g.status == "blocked") need(!g.blocked_reason.empty() && (g.next_review_at || !g.wake_trigger.empty()), "blocked goal lacks reason/wakeup");
        need(std::isfinite(g.urgency) && g.urgency >= 0 && std::isfinite(g.importance) && g.importance >= 0, "invalid goal priority");
    }
    for (const auto& g : s.goals) for (const auto& parent : g.parent_goal_ids)
        need(parent != g.id && goal_ids.contains(parent), "dangling/self goal parent");
    // Parent cycles cannot be used as implicit plans or infinite recursive dependencies.
    std::map<Id, unsigned> marks;
    std::map<Id, const Goal*> by_id;
    for (const auto& g : s.goals) by_id[g.id] = &g;
    std::function<void(const Id&)> visit = [&](const Id& id) {
        need(marks[id] != 1, "cyclic goal parents");
        if (marks[id] == 2) return;
        marks[id] = 1;
        for (const auto& parent : by_id.at(id)->parent_goal_ids) visit(parent);
        marks[id] = 2;
    };
    for (const auto& g : s.goals) visit(g.id);
    if (s.active_plan) {
        const auto& p = *s.active_plan;
        ident(p.id); tick(p.created_at); ids(p.goal_ids); ids(p.source_refs);
        need(!p.goal_ids.empty(), "plan without goal");
        for (const auto& id : p.goal_ids) need(goal_ids.contains(id), "plan references missing goal");
        choice(p.status,{"candidate","active","running","waiting_external","completed","cancelled","failed","suspended"},"plan status");
        need(s.plan_cursor <= p.steps.size() && p.steps.size() <= 12, "invalid plan cursor/step budget");
        if (p.score) need(std::isfinite(*p.score), "non-finite plan score");
        need(p.assumptions.is_array(), "plan assumptions must be an array"); finite(p.assumptions);
        std::set<Id> steps;
        for (const auto& step : p.steps) {
            ident(step.id); ident(step.method); ids(step.source_refs);
            need(steps.insert(step.id).second, "duplicate plan step");
            need(step.duration >= 1 && step.duration < max_minute, "invalid plan step duration");
            need(step.command.actor == s.actor, "foreign plan command");
            need(step.command.sequence >= 0 && step.command.sequence <= max_minute, "invalid planned sequence");
            need(step.command.args.is_object(), "invalid planned command arguments"); finite(step.command.args);
            const auto& type = step.command.type;
            need(type.size() == 3 && type[0] == 'A' && type[1] >= '0' && type[1] <= '3' &&
                 type[2] >= '0' && type[2] <= '9' && type >= "A01" && type <= "A32", "invalid planned action type");
            for (const auto& [id, version] : step.command.expected_versions) { ident(id); need(version >= 1, "invalid expected item version"); }
        }
    } else need(s.plan_cursor == 0, "cursor without active plan");
    std::set<Id> sessions, session_keys;
    for (const auto& r : s.session_runtimes) {
        ident(r.id); ident(r.proposal);
        need(r.version >= 1 && r.version < max_minute, "invalid session version");
        need(sessions.insert(r.id).second && session_keys.insert(commitment_key(r.proposal,r.version)).second, "duplicate session runtime");
        choice(r.status,{"created","offered","waiting_response","agreed","preparing","travelling","waiting_start","running","completed",
                         "declined","expired_offer","cancelled","interrupted","missed_window","execution_failed"},"session status");
        tick(r.updated_at); maybe_tick(r.first_wait); tick(r.effect_minutes); ids(r.source_refs);
    }
    std::set<Id> calendar;
    for (const auto& c : s.calendar_intents) {
        ident(c.id); need(calendar.insert(c.id).second && c.actor == s.actor,"duplicate/foreign calendar entry");
        tick(c.start); tick(c.end); tick(c.travel_buffer); need(c.start < c.end, "empty/reversed calendar interval");
        choice(c.status,{"tentative","confirmed","cancelled","completed"},"calendar status");
        if (!c.goal_id.empty()) need(goal_ids.contains(c.goal_id), "calendar missing goal");
        if (!c.session_id.empty()) need(sessions.contains(c.session_id), "calendar missing session");
    }
    for (const auto& [id, until] : s.cooldowns) { ident(id); tick(until); }
    std::set<Id> roots;
    for (const auto& o : s.outcome_history) {
        ident(o.root); ident(o.partner); ident(o.activity_family); ident(o.source_ref); tick(o.learned_at);
        need(roots.insert(o.root).second, "duplicate outcome root");
        choice(o.outcome,{"accepted","declined","countered","timeout","completed","interrupted","unknown"},"outcome status");
    }
    for (const auto& [id, cursor] : s.fair_scan_cursor) {
        ident(id); need(cursor <= static_cast<std::uint64_t>(max_minute), "invalid fair scan cursor");
    }
    std::set<Id> commands, notifications;
    for (const auto& p : s.pending_command_links) {
        ident(p.command_id); tick(p.submitted_at);
        need(p.command_id.starts_with(s.actor + "@") && commands.insert(p.command_id).second, "foreign/duplicate pending command");
        if (!p.goal_id.empty()) need(goal_ids.contains(p.goal_id), "pending command missing goal");
        if (!p.plan_id.empty()) need(s.active_plan && s.active_plan->id == p.plan_id, "pending command missing plan");
        if (!p.session_id.empty()) need(sessions.contains(p.session_id), "pending command missing session");
    }
    for (const auto& n : s.pending_notifications) {
        ident(n.id); ident(n.kind); ident(n.source_ref); tick(n.received_at);
        need(notifications.insert(n.id).second, "duplicate pending notification");
    }
    if (s.last_own_state) { detail::validate_own_state(*s.last_own_state); need(s.last_own_state->actor == s.actor,"foreign own snapshot"); }
}
namespace detail {
void validate_own_state(const OwnState& s) {
    actor_id(s.actor); tick(s.tick); choice(s.profile,{"baseline","extended"},"world profile");
    choice(s.position.kind,{"at","transit","stranded"},"position kind");
    tick(s.position.started); tick(s.position.due); ratio(s.position.progress); ratio(s.position.base_progress);
    if (s.position.kind == "at") need(!s.position.place.empty() && s.position.edge.empty(), "invalid own at position");
    else need(s.position.place.empty() && !s.position.edge.empty(), "invalid own transit position");
    need(s.money >= 0 && s.money <= max_money, "invalid own money");
    for (double d : {s.health,s.fatigue,s.pain,s.impairment,s.stress,s.fear,s.selfesteem,s.attention}) ratio(d);
    ratio(s.wellbeing, static_cast<double>(max_money));
    need(std::isfinite(s.capacity_kg) && s.capacity_kg > 0 && s.capacity_kg <= 100000, "invalid own capacity");
    need(std::isfinite(s.base_speed) && s.base_speed > 0 && s.base_speed <= 1000, "invalid own speed");
    need(s.horizon >= 1 && s.horizon <= 10080 && s.search_width >= 1 && s.search_width <= 128, "invalid own cognitive budget");
    for (const auto& [id,n] : s.needs) {
        choice(id,{"N01","N02","N03","N04","N05"},"need id"); ratio(n.value,100);
        need(0 < n.critical && n.critical < n.activation && n.activation < n.target && n.target <= 100, "invalid own need thresholds");
        for (double d : {n.awake_drain,n.sleep_drain,n.sleep_gain}) ratio(d,1000);
    }
    for (const auto& id : {"N01","N02","N03","N04"}) need(s.needs.contains(id),"missing own baseline need");
}
void validate_evidence(const Evidence& e, Minute now) {
    ident(e.id); ident(e.source); ids(e.roots); need(!e.roots.empty(), "evidence without source roots");
    ident(e.statement.subject); ident(e.statement.predicate); finite(e.statement.arguments);
    choice(e.kind,{"observation","claim","record","hypothesis"},"evidence kind");
    tick(e.learned_at); need(e.learned_at <= now, "evidence not yet learned");
    tick(e.statement.valid_from);
    need(e.statement.valid_until == -1 || (e.statement.valid_until >= e.statement.valid_from && e.statement.valid_until < max_minute), "invalid evidence interval");
    ratio(e.confidence); ratio(e.prior); ratio(e.severity);
    need(std::isfinite(e.half_life) && e.half_life >= 0, "invalid evidence half-life");
}
void validate_receipt(const LocalReceipt& r, const Id& actor, Minute now) {
    ident(r.id); need(r.actor == actor, "foreign local receipt"); tick(r.received); tick(r.observed_at);
    need(r.received <= r.observed_at && r.observed_at <= now,"future receipt");
    choice(r.status,{"queued","started","completed","interrupted","rejected"},"receipt status");
    const auto c=decode<Command>(r.request);
    need(canonical(encode(c)) == canonical(r.request), "incomplete receipt request");
    need(c.actor == actor && c.sequence >= 1 && c.sequence <= max_minute && command_key(c) == r.id,"receipt command mismatch");
    need(c.args.is_object(),"invalid receipt args"); finite(c.args);
}
} // namespace detail
} // namespace npc::policy
