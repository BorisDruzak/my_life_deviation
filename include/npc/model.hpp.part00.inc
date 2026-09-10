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
