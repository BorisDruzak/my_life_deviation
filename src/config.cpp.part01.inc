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
