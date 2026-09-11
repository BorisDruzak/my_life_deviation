#pragma once
#include "npc/model.hpp"

namespace npc {
// Transport only: no World/State pointer and no hidden obligation status.
inline constexpr std::size_t local_page_limit = 64;
inline constexpr std::size_t local_receipt_limit = 4;

struct ActorViewCursor {
    Id actor = {};
    std::uint64_t offset = 0;
    std::string prefix_hash = {};
    std::uint64_t inbox_offset = 0;
    std::string inbox_prefix_hash = {};
    bool inbox_next = false;
    template<class F> void visit(F&& f) {
        f("actor", actor);
        f("offset", offset);
        f("prefix_hash", prefix_hash);
        f("inbox_offset", inbox_offset);
        f("inbox_prefix_hash", inbox_prefix_hash);
        f("inbox_next", inbox_next);
    }
    template<class F> void visit(F&& f) const {
        f("actor", actor);
        f("offset", offset);
        f("prefix_hash", prefix_hash);
        f("inbox_offset", inbox_offset);
        f("inbox_prefix_hash", inbox_prefix_hash);
        f("inbox_next", inbox_next);
    }
};

struct OwnState {
    Id actor = {};
    Minute tick = 0;
    std::string profile = "baseline";
    Position position = {};
    std::map<std::string, Need> needs = {};
    Money money = 0;
    double health = 1;
    double fatigue = 0;
    double pain = 0;
    double impairment = 0;
    double stress = 0;
    double fear = 0;
    double selfesteem = .5;
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
    template<class F> void visit(F&& f) {
        f("actor", actor);
        f("tick", tick);
        f("profile", profile);
        f("position", position);
        f("needs", needs);
        f("money", money);
        f("health", health);
        f("fatigue", fatigue);
        f("pain", pain);
        f("impairment", impairment);
        f("stress", stress);
        f("fear", fear);
        f("selfesteem", selfesteem);
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
    }
    template<class F> void visit(F&& f) const {
        f("actor", actor);
        f("tick", tick);
        f("profile", profile);
        f("position", position);
        f("needs", needs);
        f("money", money);
        f("health", health);
        f("fatigue", fatigue);
        f("pain", pain);
        f("impairment", impairment);
        f("stress", stress);
        f("fear", fear);
        f("selfesteem", selfesteem);
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
    }
};

struct LocalReceipt {
    Id id = {};
    Id actor = {};
    Id action = {};
    std::string status = "queued";
    std::string reason = {};
    Minute received = 0;
    Json request = Object{};
    Minute observed_at = 0;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("actor", actor);
        f("action", action);
        f("status", status);
        f("reason", reason);
        f("received", received);
        f("request", request);
        f("observed_at", observed_at);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("actor", actor);
        f("action", action);
        f("status", status);
        f("reason", reason);
        f("received", received);
        f("request", request);
        f("observed_at", observed_at);
    }
};

struct LocalEnvelope {
    Id id;
    Id from;
    std::string channel;
    Minute delivered_at = 0;
    bool read = false;
    template<class F> void visit(F&& f) {
        f("id",id); f("from",from); f("channel",channel); f("delivered_at",delivered_at); f("read",read);
    }
    template<class F> void visit(F&& f) const {
        f("id",id); f("from",from); f("channel",channel); f("delivered_at",delivered_at); f("read",read);
    }
};
struct ActorViewDelta {
    std::string version = "ACTOR-VIEW-1";
    std::string world_config_hash = {};
    ActorViewCursor from = {};
    ActorViewCursor next = {};
    OwnState own = {};
    std::vector<Evidence> evidence = {};
    std::vector<LocalReceipt> receipts = {};
    std::vector<LocalEnvelope> inbox = {};
    std::uint64_t records_examined = 0;
    bool has_more = false;
    template<class F> void visit(F&& f) {
        f("version", version);
        f("world_config_hash", world_config_hash);
        f("from", from);
        f("next", next);
        f("own", own);
        f("evidence", evidence);
        f("receipts", receipts);
        f("inbox", inbox);
        f("records_examined", records_examined);
        f("has_more", has_more);
    }
    template<class F> void visit(F&& f) const {
        f("version", version);
        f("world_config_hash", world_config_hash);
        f("from", from);
        f("next", next);
        f("own", own);
        f("evidence", evidence);
        f("receipts", receipts);
        f("inbox", inbox);
        f("records_examined", records_examined);
        f("has_more", has_more);
    }
};

std::string local_history_seed(const Id& actor, const std::string& config_hash);
std::string local_history_next(const std::string& previous, const Evidence& evidence);
std::string local_inbox_seed(const Id& actor, const std::string& config_hash);
std::string local_inbox_next(const std::string& previous, const LocalEnvelope& envelope);
} // namespace npc
