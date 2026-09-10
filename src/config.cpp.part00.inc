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
