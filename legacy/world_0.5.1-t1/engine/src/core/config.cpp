#include "npc/config.hpp"
#include "npc/math.hpp"
#include <algorithm>
namespace npc {
    namespace {
        void finite_tree(const Json & j) {
            if (j.is_double() && ! std::isfinite(j.as_double())) throw InputError("non-finite configuration number");
            if (j.is_object()) for (const auto & v : j.as_object()) finite_tree(v.value());
            if (j.is_array()) for (const auto & v : j.as_array()) finite_tree(v);
        }
        void bounded(double x, double lo, double hi, const std::string & what) {
            if (! std::isfinite(x) || x < lo || x > hi) throw InputError("invalid " + what);
        }
        Json merged(Json base, const Json & patch) {
            if (! base.is_object() || ! patch.is_object()) return patch;
            for (const auto & kv : patch.as_object()) base.as_object()[kv.key()] = kv.value();
            return base;
        }
    }
    Config Config::load(const std::filesystem::path & directory) {
        return from_json(read_json(directory / "parameters.json"), read_json(directory / "catalog.json"));
    }
    Config Config::from_json(Json p, Json c) {
        Config out;
        out.parameters = std::move(p);
        out.catalog = std::move(c);
        for (auto key : {
            "items", "fixtures"
        }) for (const auto & j : at(out.catalog, key).as_array()) {
            auto id = str(j, "id");
            if (! out.types.emplace(id, j).second) throw InputError("duplicate type: " + id);
        }
        for (const auto & j : at(out.catalog, "recipes").as_array()) if (! out.recipes.emplace(str(j, "id"), j).second) throw InputError("duplicate recipe");
        for (const auto & j : at(out.catalog, "actions").as_array()) if (! out.actions.emplace(str(j, "id"), j).second) throw InputError("duplicate action");
        out.validate();
        out.hash = math::sha256(canonical(Object {
            {
                "parameters", out.parameters
            }, {
                "catalog", out.catalog
            }
        }));
        return out;
    }
    double Config::number(std::initializer_list < std::string_view > path) const {
        const Json * j = & parameters;
        for (auto key : path) j = & at(* j, key);
        return decode < double >(* j);
    }
    bool Config::enabled(std::string_view profile, std::string_view flag) const {
        return get < bool >(at(at(parameters, "profiles"), profile), flag);
    }
    const Json & Config::item_type(const Id & id) const {
        auto it = types.find(id);
        if (it == types.end()) throw InputError("unknown item type: " + id);
        return it->second;
    }
    void Config::validate() const {
        finite_tree(parameters);
        finite_tree(catalog);
        if (str(parameters, "version") != rules_version || str(catalog, "version") != rules_version) throw InputError("rules version mismatch");
        if (number({
            "time", "tick_minutes"
        }) != 1 || number({
            "time", "day_minutes"
        }) != 1440) throw InputError("this executor requires a one-minute step and a 1440-minute day");
        for (auto id : {
            "N01", "N02", "N03", "N04", "N05"
        }) {
            const auto & n = at(at(parameters, "needs"), id);
            auto c = get < double >(n, "critical"), a = get < double >(n, "activation"), t = get < double >(n, "target");
            if (!(0 < c && c < a && a < t && t <= 100)) throw InputError("invalid need thresholds: " + std::string(id));
            for (auto key : {
                "awake_drain_per_hour", "sleep_drain_per_hour", "sleep_net_gain_per_hour"
            }) bounded(get < double >(n, key), 0, 1000, key);
        }
        for (const auto & kv : at(parameters, "thresholds").as_object()) if (kv.value().is_object()) {
            auto on = get < double >(kv.value(), "on"), off = get < double >(kv.value(), "off");
            if (!(0 <= off && off < on && on <= 1)) throw InputError("invalid hysteresis");
        }
        for (auto field : {
            "traits", "norm_prices"
        }) for (const auto & kv : at(parameters, field).as_object()) bounded(decode < double >(kv.value()), 0, 1, std::string(kv.key()));
        for (auto profile : {
            "baseline", "extended"
        }) for (auto flag : {
            "intimacy", "interest_learning", "skill_learning", "variable_movement_speed"
        }) static_cast < void >(enabled(profile, flag));
        for (auto group : {
            "emotions", "physical", "learning", "repetition", "money", "drive"
        }) for (const auto & kv : at(parameters, group).as_object()) if (kv.value().is_number()) {
            auto x = decode < double >(kv.value());
            if (std::string(kv.key()).find("half_life") != std::string::npos && x <= 0) throw InputError("half life must be positive");
        }
        if (number({
            "repetition", "decay_time_constant_minutes"
        }) <= 0 || number({
            "repetition", "minutes_per_unit"
        }) <= 0 || number({
            "drive", "pressure_growth_denominator_minutes"
        }) <= 0) throw InputError("invalid divisor");
        if (number({
            "physical", "incapacitated_at"
        }) >= number({
            "physical", "capable_again_at"
        })) throw InputError("invalid capability hysteresis");
        if (number({
            "money", "wage_denominator_minutes"
        }) != 60) throw InputError("wage denominator must be 60");
        for (const auto & kv : at(at(parameters, "physical"), "fatigue_modes").as_object()) {
            if (! kv.value().is_array() || kv.value().as_array().size() != 2) throw InputError("fatigue mode needs load/recovery pair");
            bounded(decode < double >(kv.value().as_array()[0]), 0, 10, "fatigue load");
            bounded(decode < double >(kv.value().as_array()[1]), 0, 10, "fatigue recovery");
        }
        for (auto mode : {
            "ordinary", "walk", "work", "sport", "rest", "sleep", "forced_rest"
        }) static_cast < void >(at(at(at(parameters, "physical"), "fatigue_modes"), mode));
        bounded(number({
            "physical", "fatigue_damage_start"
        }), .01, .99, "fatigue damage start");
        bounded(number({
            "physical", "movement_min_speed"
        }), .001, 1000, "minimum speed");
        if (actions.size() != 32) throw InputError("A01..A32 catalogue required");
        for (int i = 1; i <= 32; ++ i) {
            auto id = std::string(i < 10 ? "A0" : "A") + std::to_string(i);
            if (! actions.contains(id)) throw InputError("missing action " + id);
        }
        for (const auto &[id, t] : types) {
            bounded(get_or < double >(t, "mass_kg", 0), 0, 100000, "mass");
            if (get_or < bool >(t, "edible", false)) {
                if (get < std::int64_t >(t, "total_units") <= 0) throw InputError("invalid food units");
                bounded(get < double >(t, "satiety_total"), 0, 10000, "nutrition");
            }
        }
        for (const auto &[id, r] : recipes) {
            if (get < Minute >(r, "duration") < 1) throw InputError("invalid recipe duration");
            bounded(get < double >(r, "min_skill"), 0, 1, "skill");
        }
    }
    Actor make_actor(const Id & id, const Id & place, const Config & c) {
        Actor a;
        a.id = id;
        a.name = id;
        a.account = id;
        a.position.place = place;
        a.traits = decode < std::map < std::string, double >>(at(c.parameters, "traits"));
        a.norm_prices = decode < std::map < std::string, double >>(at(c.parameters, "norm_prices"));
        a.capacity_kg = c.number({
            "physical", "carry_capacity_kg"
        });
        a.attention = c.number({
            "perception", "attention_base"
        });
        for (auto idn : {
            "N01", "N02", "N03", "N04"
        }) {
            const auto & v = at(at(c.parameters, "needs"), idn);
            Need n;
            n.awake_drain = get < double >(v, "awake_drain_per_hour");
            n.sleep_drain = get < double >(v, "sleep_drain_per_hour");
            n.sleep_gain = get < double >(v, "sleep_net_gain_per_hour");
            n.critical = get < double >(v, "critical");
            n.activation = get < double >(v, "activation");
            n.target = get < double >(v, "target");
            a.needs[idn] = n;
        }
        a.known_methods = {
            "A01", "A02", "A03", "A04", "A05", "A06", "A07", "A09", "A11", "A12", "A13", "A14", "A15", "A16", "A17", "A18",
            "A19", "A20", "A22", "A23", "A24", "A25", "A26", "A27", "A28", "A29", "R01"
        };
        a.known_topics = {
            "meal", "canned", "snack", "conversation", "reading", "sport", "rest"
        };
        a.skills["repair"] = .5;
        a.skills["cooking"] = .2;
        a.drives["appropriation"] = {
            0, 0
        };
        return a;
    }
    Item make_item(const Id & id, const Id & type, const Id & owner, const Placement & placement, const Config & c) {
        const auto & d = c.item_type(type);
        Item x;
        x.id = id;
        x.type = type;
        x.owner = owner;
        x.placement = placement;
        x.total_units = get_or < std::int64_t >(d, "total_units", 0);
        x.remaining_units = x.total_units;
        x.open = get_or < bool >(d, "is_open", true);
        x.condition = get_or < double >(d, "condition", 1);
        return x;
    }
    State scenario_state(const Config & c, const Json & raw) {
        if (! raw.is_object()) throw InputError("scenario.world must be an object");
        Json j = raw;
        auto profile = text(j, "profile", "baseline");
        auto actors = has(j, "actors") ? at(j, "actors").as_object() : Object {};
        for (auto & kv : actors) {
            Json patch = kv.value();
            auto id = std::string(kv.key());
            auto base = encode(make_actor(id, "", c));
            if (has(patch, "needs")) {
                Json needs = at(base, "needs");
                for (const auto & nk : at(patch, "needs").as_object()) {
                    const auto key = std::string(nk.key());
                    if (key == "N05" && ! has(needs, key)) {
                        const auto & v = at(at(c.parameters, "needs"), key);
                        Need n;
                        n.awake_drain = get < double >(v, "awake_drain_per_hour");
                        n.sleep_drain = get < double >(v, "sleep_drain_per_hour");
                        n.critical = get < double >(v, "critical");
                        n.activation = get < double >(v, "activation");
                        n.target = get < double >(v, "target");
                        needs.as_object()[key] = encode(n);
                    }
                    if (! has(needs, key)) throw InputError("unknown need " + key);
                    if (nk.value().is_number()) needs.as_object()[key].as_object()["value"] = nk.value();
                    else needs.as_object()[key] = merged(at(needs, key), nk.value());
                }
                patch.as_object()["needs"] = needs;
            }
            for (auto key : {
                "traits", "norm_prices"
            }) if (has(patch, key)) patch.as_object()[key] = merged(at(base, key), at(patch, key));
            kv.value() = merged(base, patch);
        }
        j.as_object()["actors"] = actors;
        if (has(j, "items")) for (auto & kv : j.as_object()["items"].as_object()) {
            auto x = kv.value();
            auto base = encode(make_item(std::string(kv.key()), str(x, "type"), text(x, "owner"), decode < Placement >(at(x,
            "placement")), c));
            kv.value() = merged(base, x);
        }
        State s = decode < State >(j);
        s.profile = profile;
        if (! s.config_hash.empty() && s.config_hash != c.hash) throw InputError("scenario config hash mismatch");
        s.config_hash = c.hash;
        for (auto &[id, a] : s.actors) {
            if (! s.accounts.contains(a.account)) s.accounts[a.account] = 0;
            if (! has(at(at(raw, "actors"), id), "wellbeing")) a.wellbeing = static_cast < double >(s.accounts.at(a.account));
            for (auto &[n, x] : a.needs) {
                x.active = x.value <= x.activation;
                x.critical_active = x.value <= x.critical;
            }
        }
        s.genesis_money = 0;
        for (const auto &[id, b] : s.accounts) {
            if (b < 0 || b > max_money || s.genesis_money > max_money - b) throw InputError("money exceeds supported range");
            s.genesis_money += b;
        }
        return s;
    }
}
