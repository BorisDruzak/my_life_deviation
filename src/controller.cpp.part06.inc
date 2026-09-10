            acquire(d, x, str(objective, "item_type"), get_or < Minute >(objective, "due", x.time + 1440));
            return d;
        }
        if (mode == "repair") {
            auto target = str(objective, "target");
            if (has(view, "own_jobs")) for (const auto & j : at(view, "own_jobs").as_array()) if (text(j, "target") == target) {
                if (text(j, "status") == "completed") { d.goal = "task_completed"; return d; }
                d.goal = "resume_repair";
                d.command = x.cmd("A21", Object {{"job", str(j, "id")}}, d.goal);
                return d;
            }
            std::string tool, kit;
            for (auto &[id, i] : x.inventory) {
                if (str(i, "type") == "I05") tool = id;
                if (str(i, "type") == "I06") kit = id;
            }
            if (tool.empty() || kit.empty()) {
                d.goal = "repair_inputs";
                acquire(d, x, tool.empty() ? "I05" : "I06", get_or < Minute >(objective, "due", x.time + 1440));
                return d;
            }
            d.goal = "repair";
            d.command = x.cmd("A21", Object {{"recipe", "R02"}, {"target", target}, {"tools", Array {tool}}, {"materials", Array {kit}}}, d.goal);
            return d;
        }
        const double projected_food = food.value - food.awake_drain * static_cast < double >(x.actor.horizon) / 60;
        if (food_item.empty() && projected_food <= food.activation) {
            d.goal = food.value <= food.activation ? "get_food" : "prepare_food";
            acquire(d, x, "I01", x.time + std::max < Minute >(30, x.actor.horizon));
            if (d.command) return d;
        }
        if (x.actor.fatigue >= .75 || x.actor.stress >= .6) {
            d.goal = "rest";
            d.command = x.cmd("A23", Object {{"minutes", 60}}, d.goal);
            return d;
        }
        if (has(view, "employment")) for (const auto & j : at(view, "employment").as_array()) {
            auto contract = decode < Contract >(j);
            for (const auto & window : contract.windows) if (window[0] <= x.time % 1440 && x.time % 1440 < window[1]) {
                auto it = x.known_items.find(contract.workplace);
                if (it == x.known_items.end()) continue;
                auto route = x.route(str(it->second.statement.arguments, "place"));
                if (! route.possible) continue;
                d.goal = "work";
                d.command = route.first.empty() ? x.cmd("A10", Object {{"contract", contract.id}, {"minutes", std::min < Minute >(60, window[1] - x.time % 1440)}}, d.goal) : x.cmd("A02", Object {{"edge", route.first}}, d.goal);
                return d;
            }
        }
        if (x.actor.needs.at("N04").active) {
            for (auto &[id, i] : x.inventory) if (has(config.item_type(str(i, "type")), "pleasure_per_30_min")) {
                d.goal = "leisure";
                d.command = x.cmd("A16", Object {{"item", id}, {"minutes", 30}}, d.goal);
                return d;
            }
        }
        d.goal = background_wait.empty() ? "idle" : background_wait;
        d.command = x.cmd("A01", Object {
            {
