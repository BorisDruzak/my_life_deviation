                if (item(id).placement.kind == "in" && item(id).placement.ref == dst) contained -= item_mass(id);
                need(contained + item_mass(id) <= get < double >(t, "capacity_kg") + 1e-9, "container capacity");
                lock("item/" + dst);
            }
        } else if (c.type == "A06") {
            d = 3;
            auto id = str(c.args, "item"), sid = str(c.args, "shop");
            need(state_.shops.contains(sid), "shop unavailable");
            const auto & sh = state_.shops.at(sid);
            const auto & i = item(id);
            need(sh.stock.contains(id) && i.placement.kind == "at" && i.placement.ref == sh.place && at_place(c.actor, sh.place),
            "item not in accessible stock");
            need(i.owner == sid || i.owner == sh.account, "stock owner changed");
            need(open_interval(sh.place, state_.time, state_.time + d), "service does not fit opening hours");
            a.price = sh.stock.at(id);
            need(available(actor.account) >= a.price, "not enough available money");
            lock("money/" + actor.account, state_.accounts.at(actor.account), a.price);
            lock("item/" + id);
            lock("shop/" + sid, sh.slots);
            takespace(c.actor, id);
            a.captured = Object {
                {
                    "owner", i.owner
                }
            };
        } else if (c.type == "A07") {
            auto id = str(c.args, "item");
            allowed(id);
            const auto & i = item(id);
            need(get_or < bool >(config_.item_type(i.type), "edible", false) && i.remaining_units > 0, "not edible/empty");
            need(holder(id) == c.actor, "food must be in own accessible inventory");
            d = std::min < std::int64_t >(60, i.remaining_units);
            lock("item/" + id);
        } else if (c.type == "A08" || c.type == "A21") {
            Job j;
            if (has(c.args, "job")) {
                auto jid = str(c.args, "job");
                need(state_.jobs.contains(jid), "unknown job");
                j = state_.jobs.at(jid);
                need(j.creator == c.actor && j.status == "paused" && j.active_action.empty(), "job not resumable");
                need(at_place(c.actor, j.place), "wrong job location");
            } else {
                j.id = a.id + "/job";
                j.creator = c.actor;
                j.place = actor.position.place;
                j.recipe = text(c.args, "recipe", c.type == "A08" ? "R01" : "R02");
                need(config_.recipes.contains(j.recipe), "unknown recipe");
                need((c.type == "A08") ==(j.recipe == "R01"), "wrong recipe action");
                j.required = get < Minute >(config_.recipes.at(j.recipe), "duration");
                j.fixture = text(c.args, "fixture");
                j.target = text(c.args, "target");
                j.materials = get_or < std::vector < Id >>(c.args, "materials", {});
                j.tools = get_or < std::vector < Id >>(c.args, "tools", {});
                unique_ids(j.materials);
                unique_ids(j.tools);
                if (j.recipe == "R01") {
                    need(j.materials.size() == 1 && item(j.materials.front()).type == "I04", "one ingredient pack required");
                }
                if (j.recipe == "R02") {
                    need(j.materials.size() == 1 && item(j.materials.front()).type == "I06" && j.tools.size() == 1, "repair material and tool required");
                }
                if (j.recipe == "R03") {
                    need(j.materials.empty() && j.tools.size() == 1, "one fastening object required");
                }
                for (auto & id : j.materials) allowed(id);
                for (const auto &[other, job] : state_.jobs) if (! j.target.empty() && job.target == j.target && job.status != "completed") throw InputError("target already has unfinished work");
                new_job = j;
            }
