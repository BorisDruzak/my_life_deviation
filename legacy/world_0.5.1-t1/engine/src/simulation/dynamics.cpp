#include "npc/dynamics.hpp"
#include "npc/math.hpp"
namespace npc {
    namespace {
        double lookup(const std::map < std::string, double > & m, const std::string & k, double def = 0) {
            auto it = m.find(k);
            return it == m.end() ? def : it->second;
        }
    }
    void apply_minute(const Config & c, const std::string & profile, Actor & next, const Actor & old, const MinuteEffects & effect) {
        if (! old.alive) return;
        const bool sleeping = effect.mode == "sleep";
        const bool rest = sleeping || effect.mode == "rest" || effect.mode == "forced_rest";
        for (const auto &[key, n] : old.needs) {
            if (! n.enabled) continue;
            double d = sleeping && key == "N02" ? n.sleep_gain / 60 : -(sleeping ? n.sleep_drain : n.awake_drain) / 60;
            next.needs.at(key).value = math::clip(n.value + d + lookup(effect.needs, key), 0, 100);
        }
        double leisure = 0;
        double exposure_sum = 0;
        for (const auto &[g, e] : effect.exposure) {
            if (e < 0 || e > 1) throw InvariantError("invalid exposure");
            exposure_sum += e;
            auto r = lookup(old.repetition, g), i = lookup(old.interests, g, .5);
            auto rate = lookup(effect.pleasure_rate, g);
            leisure += math::pleasure(rate, e, i, old.traits.at("novelty"), r);
            if (c.enabled(profile, "interest_learning")) {
                double valence = math::clip(c.number({
                    "learning", "valence_base"
                }) + c.number({
                    "learning", "valence_success"
                }) + c.number({
                    "learning", "valence_novelty"
                }) /(1 + r));
                next.interests[g] = math::clip(i + c.number({
                    "learning", "interest_per_hour"
                }) * e / 60 *(valence - i));
            }
        }
        if (exposure_sum > 1 + 1e-12) throw InvariantError("attention exceeds one minute");
        if (old.needs.contains("N04") && old.needs.at("N04").enabled) {
            const auto & n = old.needs.at("N04");
            next.needs["N04"].value = math::clip(n.value -(sleeping ? n.sleep_drain : n.awake_drain) / 60 + lookup(effect.needs,
            "N04") + leisure, 0, 100);
        }
        const auto tau = c.number({
            "repetition", "decay_time_constant_minutes"
        }), repm = c.number({
            "repetition", "minutes_per_unit"
        });
        for (const auto &[g, r] : old.repetition) next.repetition[g] = r * std::exp(- 1 / tau) + lookup(effect.exposure,
        g) / repm;
        for (const auto &[g, e] : effect.exposure) if (! old.repetition.contains(g)) next.repetition[g] = e / repm;
        const auto & food = old.needs.at("N01");
        const auto & sleep = old.needs.at("N02");
        auto df = math::clip((food.critical - food.value) / food.critical), ds = math::clip((sleep.critical - sleep.value) / sleep.critical);
        const auto & rates = at(at(at(c.parameters, "physical"), "fatigue_modes"), effect.mode).as_array();
        auto load = decode < double >(rates.at(0)), recovery = decode < double >(rates.at(1));
        next.fatigue = math::clip(old.fatigue +(load *(1 + c.number({
            "physical", "fatigue_deficit_multiplier_food"
        }) * df + c.number({
            "physical", "fatigue_deficit_multiplier_sleep"
        }) * ds) - recovery) / 60);
        auto start = c.number({
            "physical", "fatigue_damage_start"
        });
        auto over = std::max(0.0,(old.fatigue - start) /(1 - start));
        next.health = math::clip(old.health +(c.number({
            "physical", "health_recovery_per_hour"
        }) *(1 - old.health) *(rest && df == 0 ? 1 : 0) - c.number({
            "physical", "hunger_health_loss_per_hour"
        }) * df * df - c.number({
            "physical", "overfatigue_health_loss_per_hour"
        }) * over * over) / 60);
        next.pain = math::half_decay(old.pain, 1, c.number({
            "physical", "pain_half_life_minutes"
        }));
        next.impairment = math::half_decay(old.impairment, 1, c.number({
            "physical", "impairment_half_life_minutes"
        }));
        double relief = 0;
        const auto & rel = at(at(c.parameters, "emotions"), "stress_relief_per_minute");
        if (has(rel, effect.mode)) relief = get < double >(rel, effect.mode);
        next.stress = math::clip(math::half_decay(old.stress, 1, c.number({
            "emotions", "stress_half_life_minutes"
        })) - relief);
        const auto need_rate = get < double >(at(at(c.parameters, "profiles"), profile), "need_stress_rate");
        next.stress = math::clip(next.stress + need_rate * std::max(df, ds) / 60);
        for (const auto &[who, g] : old.anger) next.anger[who] = math::half_decay(g, 1, c.number({
            "emotions", "anger_half_life_minutes"
        }));
        next.selfesteem = math::ema(old.selfesteem, old.selfesteem_base, c.number({
            "emotions", "selfesteem_half_life_minutes"
        }));
        for (const auto &[key, d] : old.drives) next.drives[key].pressure = d.intensity == 0 ? 0 : math::clip(d.pressure + d.intensity / c.number({
            "drive", "pressure_growth_denominator_minutes"
        }));
        if (! effect.practiced_skill.empty() && c.enabled(profile, "skill_learning")) {
            auto skill = lookup(old.skills, effect.practiced_skill);
            next.skills[effect.practiced_skill] = math::clip(skill + c.number({
                "learning", "skill_per_hour"
            }) / 60 * effect.task_fit *(1 - skill));
        }
    }
    Actor predict_physiology(const Config & c, Actor assumed, Minute minutes, const std::string & mode) {
        if (minutes < 0 || minutes > 10080) throw InputError("forecast horizon out of range");
        if (! has(at(at(c.parameters, "physical"), "fatigue_modes"), mode)) throw InputError("unknown forecast mode");
        for (Minute t = 0; t < minutes; ++ t) {
            Actor old = assumed;
            MinuteEffects e;
            e.mode = mode;
            apply_minute(c, "baseline", assumed, old, e);
        }
        return assumed;
    }
}
