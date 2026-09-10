#pragma once
#include "npc/json.hpp"
namespace npc {
struct Need {
    double value = 70;
    double awake_drain = 0;
    double sleep_drain = 0;
    double sleep_gain = 0;
    double critical = 10;
    double activation = 35;
    double target = 80;
    bool enabled = true;
    bool active = false;
    bool critical_active = false;
    template<class F> void visit(F&& f) {
        f("value", value);
        f("awake_drain", awake_drain);
        f("sleep_drain", sleep_drain);
        f("sleep_gain", sleep_gain);
        f("critical", critical);
        f("activation", activation);
        f("target", target);
        f("enabled", enabled);
        f("active", active);
        f("critical_active", critical_active);
    }
    template<class F> void visit(F&& f) const {
        f("value", value);
        f("awake_drain", awake_drain);
        f("sleep_drain", sleep_drain);
        f("sleep_gain", sleep_gain);
        f("critical", critical);
        f("activation", activation);
        f("target", target);
        f("enabled", enabled);
        f("active", active);
        f("critical_active", critical_active);
    }
};
struct Position {
    std::string kind = "at";
    Id place = {};
    Id edge = {};
    Id from = {};
    Id to = {};
    Minute started = 0;
    Minute due = 0;
    double base_progress = 0;
    double progress = 0;
    template<class F> void visit(F&& f) {
        f("kind", kind);
        f("place", place);
        f("edge", edge);
        f("from", from);
        f("to", to);
        f("started", started);
        f("due", due);
        f("base_progress", base_progress);
        f("progress", progress);
    }
    template<class F> void visit(F&& f) const {
        f("kind", kind);
        f("place", place);
        f("edge", edge);
        f("from", from);
        f("to", to);
        f("started", started);
        f("due", due);
        f("base_progress", base_progress);
        f("progress", progress);
    }
};
struct Relation {
    double affection = 0;
    double trust = .5;
    double tension = 0;
    double attraction = 0;
    bool affection_high = false;
    bool tension_high = false;
    Minute contact_expected = -1;
    template<class F> void visit(F&& f) {
        f("affection", affection);
        f("trust", trust);
        f("tension", tension);
        f("attraction", attraction);
        f("affection_high", affection_high);
        f("tension_high", tension_high);
        f("contact_expected", contact_expected);
    }
    template<class F> void visit(F&& f) const {
        f("affection", affection);
        f("trust", trust);
        f("tension", tension);
        f("attraction", attraction);
        f("affection_high", affection_high);
        f("tension_high", tension_high);
        f("contact_expected", contact_expected);
    }
};
struct Drive {
    double intensity = 0;
    double pressure = 0;
    template<class F> void visit(F&& f) {
        f("intensity", intensity);
        f("pressure", pressure);
    }
    template<class F> void visit(F&& f) const {
        f("intensity", intensity);
        f("pressure", pressure);
    }
};
struct Statement {
    Id subject = {};
    std::string predicate = {};
    Json arguments = Object{};
    bool polarity = true;
    Minute valid_from = 0;
    Minute valid_until = -1;
    template<class F> void visit(F&& f) {
        f("subject", subject);
        f("predicate", predicate);
        f("arguments", arguments);
        f("polarity", polarity);
        f("valid_from", valid_from);
        f("valid_until", valid_until);
    }
    template<class F> void visit(F&& f) const {
        f("subject", subject);
        f("predicate", predicate);
        f("arguments", arguments);
        f("polarity", polarity);
        f("valid_from", valid_from);
        f("valid_until", valid_until);
    }
};
struct Evidence {
    Id id = {};
    Statement statement = {};
    std::string kind = "observation";
    Minute learned_at = 0;
    double confidence = 1;
    Id source = {};
    std::vector<Id> roots = {};
    double prior = .5;
    double half_life = 0;
    double severity = 0;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("statement", statement);
        f("kind", kind);
        f("learned_at", learned_at);
        f("confidence", confidence);
        f("source", source);
        f("roots", roots);
        f("prior", prior);
        f("half_life", half_life);
        f("severity", severity);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("statement", statement);
        f("kind", kind);
        f("learned_at", learned_at);
        f("confidence", confidence);
        f("source", source);
        f("roots", roots);
        f("prior", prior);
        f("half_life", half_life);
        f("severity", severity);
    }
};
struct Actor {
    Id id = {};
    std::string name = {};
    Minute birth_day = -10950;
    Id account = {};
    Position position = {};
    std::map<std::string,Need> needs = {};
    std::map<std::string,double> traits = {};
    std::map<std::string,double> norm_prices = {};
    std::map<std::string,double> interests = {};
    std::map<std::string,double> repetition = {};
    std::map<std::string,double> skills = {};
    std::set<std::string> known_methods = {};
    std::set<std::string> known_topics = {};
    std::map<std::string,double> anger = {};
    std::map<std::string,Relation> relations = {};
    std::map<std::string,Drive> drives = {};
    std::vector<Evidence> beliefs = {};
    std::vector<Id> inbox = {};
    std::map<std::string,bool> latches = {};
    double health = 1;
    double fatigue = 0;
    double pain = 0;
    double impairment = 0;
    double stress = 0;
    double fear = 0;
    double selfesteem = .5;
    double selfesteem_base = .5;
    double wellbeing = 0;
    double capacity_kg = 15;
    double base_speed = 1;
    double attention = .8;
    bool alive = true;
    bool capable = true;
    bool heavy_allowed = true;
    Id active_action = {};
    Minute horizon = 180;
    std::int64_t search_width = 8;
    std::map<std::string,std::int64_t> outcome_counts = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("name", name);
        f("birth_day", birth_day);
        f("account", account);
        f("position", position);
        f("needs", needs);
        f("traits", traits);
        f("norm_prices", norm_prices);
        f("interests", interests);
        f("repetition", repetition);
        f("skills", skills);
        f("known_methods", known_methods);
        f("known_topics", known_topics);
        f("anger", anger);
        f("relations", relations);
        f("drives", drives);
        f("beliefs", beliefs);
        f("inbox", inbox);
        f("latches", latches);
        f("health", health);
        f("fatigue", fatigue);
        f("pain", pain);
        f("impairment", impairment);
        f("stress", stress);
        f("fear", fear);
        f("selfesteem", selfesteem);
        f("selfesteem_base", selfesteem_base);
        f("wellbeing", wellbeing);
        f("capacity_kg", capacity_kg);
        f("base_speed", base_speed);
        f("attention", attention);
        f("alive", alive);
        f("capable", capable);
        f("heavy_allowed", heavy_allowed);
        f("active_action", active_action);
        f("horizon", horizon);
        f("search_width", search_width);
        f("outcome_counts", outcome_counts);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("name", name);
        f("birth_day", birth_day);
        f("account", account);
        f("position", position);
        f("needs", needs);
        f("traits", traits);
        f("norm_prices", norm_prices);
        f("interests", interests);
        f("repetition", repetition);
        f("skills", skills);
        f("known_methods", known_methods);
        f("known_topics", known_topics);
        f("anger", anger);
        f("relations", relations);
        f("drives", drives);
        f("beliefs", beliefs);
        f("inbox", inbox);
        f("latches", latches);
        f("health", health);
        f("fatigue", fatigue);
        f("pain", pain);
        f("impairment", impairment);
        f("stress", stress);
        f("fear", fear);
        f("selfesteem", selfesteem);
        f("selfesteem_base", selfesteem_base);
        f("wellbeing", wellbeing);
        f("capacity_kg", capacity_kg);
        f("base_speed", base_speed);
        f("attention", attention);
        f("alive", alive);
        f("capable", capable);
        f("heavy_allowed", heavy_allowed);
        f("active_action", active_action);
        f("horizon", horizon);
        f("search_width", search_width);
        f("outcome_counts", outcome_counts);
    }
};
struct Place {
    Id id = {};
    std::string name = {};
    Id owner = {};
    bool private_space = false;
    bool public_entry = true;
    std::int64_t capacity = 20;
    std::vector<std::vector<Minute>> open_windows = {{0,1440}};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("name", name);
        f("owner", owner);
        f("private_space", private_space);
        f("public_entry", public_entry);
        f("capacity", capacity);
        f("open_windows", open_windows);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("name", name);
        f("owner", owner);
        f("private_space", private_space);
        f("public_entry", public_entry);
        f("capacity", capacity);
        f("open_windows", open_windows);
    }
};
struct Edge {
    Id id = {};
    Id from = {};
    Id to = {};
    Minute minutes = 1;
    bool open = true;
    Id key = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("from", from);
        f("to", to);
        f("minutes", minutes);
        f("open", open);
        f("key", key);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("from", from);
        f("to", to);
        f("minutes", minutes);
        f("open", open);
        f("key", key);
    }
};
struct Placement {
    std::string kind = "at";
    Id ref = {};
    template<class F> void visit(F&& f) {
        f("kind", kind);
        f("ref", ref);
    }
    template<class F> void visit(F&& f) const {
        f("kind", kind);
        f("ref", ref);
    }
};
struct Item {
    Id id = {};
    Id type = {};
    Id owner = {};
    Placement placement = {};
    double condition = 1;
    std::int64_t version = 1;
    std::int64_t total_units = 0;
    std::int64_t remaining_units = 0;
    bool open = true;
    bool locked = false;
    Id key = {};
    bool important = false;
    bool secured = false;
    Id created_by = "genesis";
    template<class F> void visit(F&& f) {
        f("id", id);
        f("type", type);
        f("owner", owner);
        f("placement", placement);
        f("condition", condition);
        f("version", version);
        f("total_units", total_units);
        f("remaining_units", remaining_units);
        f("open", open);
        f("locked", locked);
        f("key", key);
        f("important", important);
        f("secured", secured);
        f("created_by", created_by);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("type", type);
        f("owner", owner);
        f("placement", placement);
        f("condition", condition);
        f("version", version);
        f("total_units", total_units);
        f("remaining_units", remaining_units);
        f("open", open);
        f("locked", locked);
        f("key", key);
        f("important", important);
        f("secured", secured);
        f("created_by", created_by);
    }
};
struct Shop {
    Id id = {};
    Id place = {};
    Id account = {};
    std::int64_t slots = 1;
    std::map<Id,Money> stock = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("place", place);
        f("account", account);
        f("slots", slots);
        f("stock", stock);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("place", place);
        f("account", account);
        f("slots", slots);
        f("stock", stock);
    }
};
struct Permission {
    Id resource = {};
    Id authority = {};
    std::set<Id> grantees = {};
    std::set<Id> denied = {};
    std::int64_t version = 1;
    template<class F> void visit(F&& f) {
        f("resource", resource);
        f("authority", authority);
        f("grantees", grantees);
        f("denied", denied);
        f("version", version);
    }
    template<class F> void visit(F&& f) const {
        f("resource", resource);
        f("authority", authority);
        f("grantees", grantees);
        f("denied", denied);
        f("version", version);
    }
};
struct Contract {
    Id id = {};
    Id worker = {};
    Id employer_account = {};
    Id workplace = {};
    Money rate_per_hour = 60;
    Money accrued_numerator = 0;
    Minute next_payout = 1440;
    Minute payout_period = 1440;
    std::vector<std::vector<Minute>> windows = {{480,1020}};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("worker", worker);
        f("employer_account", employer_account);
        f("workplace", workplace);
        f("rate_per_hour", rate_per_hour);
        f("accrued_numerator", accrued_numerator);
        f("next_payout", next_payout);
        f("payout_period", payout_period);
        f("windows", windows);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("worker", worker);
        f("employer_account", employer_account);
        f("workplace", workplace);
        f("rate_per_hour", rate_per_hour);
        f("accrued_numerator", accrued_numerator);
        f("next_payout", next_payout);
        f("payout_period", payout_period);
        f("windows", windows);
    }
};
struct Command {
    Id actor = {};
    std::int64_t sequence = 0;
    std::string type = "A01";
    Json args = Object{};
    std::map<Id,std::int64_t> expected_versions = {};
    Id intent = {};
    template<class F> void visit(F&& f) {
        f("actor", actor);
        f("sequence", sequence);
        f("type", type);
        f("args", args);
        f("expected_versions", expected_versions);
        f("intent", intent);
    }
    template<class F> void visit(F&& f) const {
        f("actor", actor);
        f("sequence", sequence);
        f("type", type);
        f("args", args);
        f("expected_versions", expected_versions);
        f("intent", intent);
    }
};
struct Receipt {
    std::string fingerprint = {};
    Json request = Object{};
    Id actor = {};
    Id action = {};
    std::string status = "queued";
    std::string public_reason = {};
    std::string debug_reason = {};
    Minute received = 0;
    template<class F> void visit(F&& f) {
        f("fingerprint", fingerprint);
        f("request", request);
        f("actor", actor);
        f("action", action);
        f("status", status);
        f("public_reason", public_reason);
        f("debug_reason", debug_reason);
        f("received", received);
    }
    template<class F> void visit(F&& f) const {
        f("fingerprint", fingerprint);
        f("request", request);
        f("actor", actor);
        f("action", action);
        f("status", status);
        f("public_reason", public_reason);
        f("debug_reason", debug_reason);
        f("received", received);
    }
};
struct Reservation {
    Id id = {};
    Id resource = {};
    Id action = {};
    Money amount = 1;
    Money capacity = 1;
    Minute until = 0;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("resource", resource);
        f("action", action);
        f("amount", amount);
        f("capacity", capacity);
        f("until", until);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("resource", resource);
        f("action", action);
        f("amount", amount);
        f("capacity", capacity);
        f("until", until);
    }
};
struct Job {
    Id id = {};
    Id recipe = {};
    Id creator = {};
    Id place = {};
    Id target = {};
    Id fixture = {};
    std::vector<Id> tools = {};
    std::vector<Id> materials = {};
    std::vector<Id> outputs = {};
    Minute worked = 0;
    Minute required = 1;
    std::string status = "paused";
    Id active_action = {};
    double recorded_loss_kg = 0;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("recipe", recipe);
        f("creator", creator);
        f("place", place);
        f("target", target);
        f("fixture", fixture);
        f("tools", tools);
        f("materials", materials);
        f("outputs", outputs);
        f("worked", worked);
        f("required", required);
        f("status", status);
        f("active_action", active_action);
        f("recorded_loss_kg", recorded_loss_kg);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("recipe", recipe);
        f("creator", creator);
        f("place", place);
        f("target", target);
        f("fixture", fixture);
        f("tools", tools);
        f("materials", materials);
        f("outputs", outputs);
        f("worked", worked);
        f("required", required);
        f("status", status);
        f("active_action", active_action);
        f("recorded_loss_kg", recorded_loss_kg);
    }
};
struct Proposal {
    Id id = {};
    std::int64_t version = 1;
    Id author = {};
    std::set<Id> participants = {};
    std::set<Id> signatures = {};
    Json terms = Object{};
    Minute created = 0;
    Minute expires = 0;
    std::string state = "offered";
    Id root = {};
    bool executed = false;
    std::vector<Json> prior_versions = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("version", version);
        f("author", author);
        f("participants", participants);
        f("signatures", signatures);
        f("terms", terms);
        f("created", created);
        f("expires", expires);
        f("state", state);
        f("root", root);
        f("executed", executed);
        f("prior_versions", prior_versions);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("version", version);
        f("author", author);
        f("participants", participants);
        f("signatures", signatures);
        f("terms", terms);
        f("created", created);
        f("expires", expires);
        f("state", state);
        f("root", root);
        f("executed", executed);
        f("prior_versions", prior_versions);
    }
};
struct Obligation {
    Id id = {};
    Id root = {};
    Id debtor = {};
    Id creditor = {};
    std::string kind = {};
    Id item = {};
    Id target = {};
    Money amount = 0;
    Money remaining = 0;
    Minute due = 0;
    bool important = false;
    std::string status = "pending";
    Minute fulfilled_at = -1;
    Minute breached_at = -1;
    std::set<Id> acknowledged = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("root", root);
        f("debtor", debtor);
        f("creditor", creditor);
        f("kind", kind);
        f("item", item);
        f("target", target);
        f("amount", amount);
        f("remaining", remaining);
        f("due", due);
        f("important", important);
        f("status", status);
        f("fulfilled_at", fulfilled_at);
        f("breached_at", breached_at);
        f("acknowledged", acknowledged);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("root", root);
        f("debtor", debtor);
        f("creditor", creditor);
        f("kind", kind);
        f("item", item);
        f("target", target);
        f("amount", amount);
        f("remaining", remaining);
        f("due", due);
        f("important", important);
        f("status", status);
        f("fulfilled_at", fulfilled_at);
        f("breached_at", breached_at);
        f("acknowledged", acknowledged);
    }
};
struct Action {
    Id id = {};
    std::string type = {};
    Id actor = {};
    std::vector<Id> participants = {};
    std::vector<Id> commands = {};
    Json args = Object{};
    Minute started = 0;
    Minute due = 0;
    Minute elapsed = 0;
    Id job = {};
    Id proposal = {};
    std::int64_t proposal_version = 0;
    Money price = 0;
    Json captured = Object{};
    Id intent = {};
    std::set<Id> entry_observers = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("type", type);
        f("actor", actor);
        f("participants", participants);
        f("commands", commands);
        f("args", args);
        f("started", started);
        f("due", due);
        f("elapsed", elapsed);
        f("job", job);
        f("proposal", proposal);
        f("proposal_version", proposal_version);
        f("price", price);
        f("captured", captured);
        f("intent", intent);
        f("entry_observers", entry_observers);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("type", type);
        f("actor", actor);
        f("participants", participants);
        f("commands", commands);
        f("args", args);
        f("started", started);
        f("due", due);
        f("elapsed", elapsed);
        f("job", job);
        f("proposal", proposal);
        f("proposal_version", proposal_version);
        f("price", price);
        f("captured", captured);
        f("intent", intent);
        f("entry_observers", entry_observers);
    }
};
struct RecordVersion {
    std::int64_t version = 1;
    Minute time = 0;
    Json content = Object{};
    std::set<Id> acl = {};
    bool public_read = false;
    bool deleted = false;
    template<class F> void visit(F&& f) {
        f("version", version);
        f("time", time);
        f("content", content);
        f("acl", acl);
        f("public_read", public_read);
        f("deleted", deleted);
    }
    template<class F> void visit(F&& f) const {
        f("version", version);
        f("time", time);
        f("content", content);
        f("acl", acl);
        f("public_read", public_read);
        f("deleted", deleted);
    }
};
struct Record {
    Id id = {};
    Id author = {};
    std::vector<RecordVersion> versions = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("author", author);
        f("versions", versions);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("author", author);
        f("versions", versions);
    }
};
struct Message {
    Id id = {};
    Id author = {};
    std::vector<Id> recipients = {};
    Json content = Object{};
    std::vector<Id> roots = {};
    std::string channel = "speech";
    Minute sent = 0;
    Minute deliver_at = 0;
    bool delivered = false;
    std::set<Id> readers = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("author", author);
        f("recipients", recipients);
        f("content", content);
        f("roots", roots);
        f("channel", channel);
        f("sent", sent);
        f("deliver_at", deliver_at);
        f("delivered", delivered);
        f("readers", readers);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("author", author);
        f("recipients", recipients);
        f("content", content);
        f("roots", roots);
        f("channel", channel);
        f("sent", sent);
        f("deliver_at", deliver_at);
        f("delivered", delivered);
        f("readers", readers);
    }
};
struct Event {
    Id uid = {};
    std::int64_t sequence = 0;
    Minute time = 0;
    std::string phase = {};
    std::string type = {};
    Id cause = {};
    Id root = {};
    std::vector<Id> participants = {};
    std::vector<Id> objects = {};
    Json data = Object{};
    Json private_data = Object{};
    std::map<Id,Json> projections = {};
    template<class F> void visit(F&& f) {
        f("uid", uid);
        f("sequence", sequence);
        f("time", time);
        f("phase", phase);
        f("type", type);
        f("cause", cause);
        f("root", root);
        f("participants", participants);
        f("objects", objects);
        f("data", data);
        f("private_data", private_data);
        f("projections", projections);
    }
    template<class F> void visit(F&& f) const {
        f("uid", uid);
        f("sequence", sequence);
        f("time", time);
        f("phase", phase);
        f("type", type);
        f("cause", cause);
        f("root", root);
        f("participants", participants);
        f("objects", objects);
        f("data", data);
        f("private_data", private_data);
        f("projections", projections);
    }
};
struct Appraisal {
    Id observer = {};
    Id target = {};
    Id root = {};
    std::string type = {};
    double trust_delta = 0;
    double tension_delta = 0;
    double stress_delta = 0;
    double anger_delta = 0;
    double esteem_delta = 0;
    template<class F> void visit(F&& f) {
        f("observer", observer);
        f("target", target);
        f("root", root);
        f("type", type);
        f("trust_delta", trust_delta);
        f("tension_delta", tension_delta);
        f("stress_delta", stress_delta);
        f("anger_delta", anger_delta);
        f("esteem_delta", esteem_delta);
    }
    template<class F> void visit(F&& f) const {
        f("observer", observer);
        f("target", target);
        f("root", root);
        f("type", type);
        f("trust_delta", trust_delta);
        f("tension_delta", tension_delta);
        f("stress_delta", stress_delta);
        f("anger_delta", anger_delta);
        f("esteem_delta", esteem_delta);
    }
};
struct External {
    Id id = {};
    Minute at = 0;
    std::int64_t priority = 0;
    Id issuer = "scenario";
    std::string kind = {};
    Json args = Object{};
    bool applied = false;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("at", at);
        f("priority", priority);
        f("issuer", issuer);
        f("kind", kind);
        f("args", args);
        f("applied", applied);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("at", at);
        f("priority", priority);
        f("issuer", issuer);
        f("kind", kind);
        f("args", args);
        f("applied", applied);
    }
};
struct State {
    std::string version = rules_version;
    std::string config_hash = {};
    std::string profile = "baseline";
    std::string seed = "world-seed-1";
    Minute time = 0;
    std::map<Id,Place> places = {};
    std::map<Id,Edge> edges = {};
    std::map<Id,Actor> actors = {};
    std::map<Id,Item> items = {};
    std::map<Id,Money> accounts = {};
    std::map<Id,Shop> shops = {};
    std::map<Id,Permission> permissions = {};
    std::map<Id,Contract> contracts = {};
    std::map<Id,Action> actions = {};
    std::map<Id,Job> jobs = {};
    std::map<Id,Proposal> proposals = {};
    std::map<Id,Obligation> obligations = {};
    std::map<Id,Record> records = {};
    std::map<Id,Message> messages = {};
    std::map<Id,Reservation> reservations = {};
    std::map<Id,Receipt> receipts = {};
    std::vector<Command> pending = {};
    std::vector<External> external = {};
    std::vector<Event> ledger = {};
    std::map<Id,std::int64_t> event_counters = {};
    std::set<Id> delivered_evidence = {};
    std::map<Id,Appraisal> appraisal_history = {};
    std::map<Id,Minute> positive_cooldowns = {};
    Money genesis_money = 0;
    Money external_money = 0;
    std::int64_t next_event_sequence = 0;
    template<class F> void visit(F&& f) {
        f("version", version);
        f("config_hash", config_hash);
        f("profile", profile);
        f("seed", seed);
        f("time", time);
        f("places", places);
        f("edges", edges);
        f("actors", actors);
        f("items", items);
        f("accounts", accounts);
        f("shops", shops);
        f("permissions", permissions);
        f("contracts", contracts);
        f("actions", actions);
        f("jobs", jobs);
        f("proposals", proposals);
        f("obligations", obligations);
        f("records", records);
        f("messages", messages);
        f("reservations", reservations);
        f("receipts", receipts);
        f("pending", pending);
        f("external", external);
        f("ledger", ledger);
        f("event_counters", event_counters);
        f("delivered_evidence", delivered_evidence);
        f("appraisal_history", appraisal_history);
        f("positive_cooldowns", positive_cooldowns);
        f("genesis_money", genesis_money);
        f("external_money", external_money);
        f("next_event_sequence", next_event_sequence);
    }
    template<class F> void visit(F&& f) const {
        f("version", version);
        f("config_hash", config_hash);
        f("profile", profile);
        f("seed", seed);
        f("time", time);
        f("places", places);
        f("edges", edges);
        f("actors", actors);
        f("items", items);
        f("accounts", accounts);
        f("shops", shops);
        f("permissions", permissions);
        f("contracts", contracts);
        f("actions", actions);
        f("jobs", jobs);
        f("proposals", proposals);
        f("obligations", obligations);
        f("records", records);
        f("messages", messages);
        f("reservations", reservations);
        f("receipts", receipts);
        f("pending", pending);
        f("external", external);
        f("ledger", ledger);
        f("event_counters", event_counters);
        f("delivered_evidence", delivered_evidence);
        f("appraisal_history", appraisal_history);
        f("positive_cooldowns", positive_cooldowns);
        f("genesis_money", genesis_money);
        f("external_money", external_money);
        f("next_event_sequence", next_event_sequence);
    }
};
struct TickReport {Minute time; std::size_t events;};
inline Id command_key(const Command& c){return c.actor+"@"+std::to_string(c.sequence);}
}
