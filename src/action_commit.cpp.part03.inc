            appraise(to, a.actor, o.root, "return");
        } else if (mode == "repay") {
            auto & o = state_.obligations.at(str(a.args, "obligation"));
            o.remaining -= a.price;
            if (o.remaining == 0) {
                satisfy_obligation(o, a.id);
                o.acknowledged.insert(to);
                appraise(to, a.actor, o.root, "return");
            }
        }
    }
    void World::complete_job(const Action & a) {
        auto & j = state_.jobs.at(a.job);
        if (j.worked != j.required || j.status == "completed") throw InvariantError("job completion repeated/early");
        if (j.recipe == "R01") {
            auto id = j.id + "/output/0";
            auto out = make_item(id, "I01", j.creator, {
                "at", j.place
            }, config_);
            out.created_by = j.id;
            if (! state_.items.emplace(id, out).second) throw InvariantError("duplicate job output");
            j.outputs.push_back(id);
            j.recorded_loss_kg = get < double >(config_.recipes.at(j.recipe), "recorded_waste_kg");
        } else if (j.recipe == "R02") {
            auto & t = state_.items.at(j.target);
            t.condition = 1;
            ++ t.version;
            j.recorded_loss_kg = .3;
        } else if (j.recipe == "R03") {
            auto & t = state_.items.at(j.target);
            t.secured = true;
            ++ t.version;
            auto & r = state_.items.at(j.tools.front());
            r.placement = {
                "installed", j.target
            };
            ++ r.version;
        }
        for (auto & id : j.materials) {
            auto & i = state_.items.at(id);
            i.placement = {
                "tombstone", j.id
            };
            i.remaining_units = 0;
            ++ i.version;
        }
        j.status = "completed";
        j.active_action.clear();
        emit(a.id, "job_completed", {
            a.actor
        }, j.outputs, Object {
            {
                "job", j.id
            }, {
                "recipe", j.recipe
            }, {
                "target", j.target
            }
        }, false, j.id);
        emit(a.id, "item_transformed", {
            a.actor
        }, j.outputs, Object {
            {
                "inputs", encode(j.materials)
            }, {
                "outputs", encode(j.outputs)
            }, {
                "recorded_loss_kg", j.recorded_loss_kg
            }
        }, false, j.id);
        for (auto &[id, o] : state_.obligations) if ((o.status == "pending" || o.status == "breached") && o.kind == "repair" && o.debtor == a.actor && o.target == j.target) satisfy_obligation(o,
        a.id);
    }
    void World::commit(const Id & id) {
        Action a = state_.actions.at(id);
        auto & actor = state_.actors.at(a.actor);
        if (a.type == "A02") {
            auto dest = actor.position.to;
            actor.position = Position {};
            actor.position.place = dest;
            emit(a.id, "arrived", {
                a.actor
            }, {}, Object {
                {
                    "place", dest
                }
            });
        } else if (a.type == "A03") {
            std::vector < Id > ids;
            if (has(a.args, "item")) {
                auto target = str(a.args, "item");
                ids.push_back(target);
                for (const auto &[x, i] : state_.items) if (i.placement.kind == "in" && i.placement.ref == target && accessible(a.actor,
                x)) ids.push_back(x);
            } else if (text(a.args, "scope", "all") == "all") for (const auto &[x, i] : state_.items) if (accessible(a.actor,
            x)) ids.push_back(x);
            for (auto & x : ids) {
                const auto & i = state_.items.at(x);
                Evidence e;
                e.id = a.id + "/inspect/" + x;
                e.source = a.actor;
                e.roots = {
                    e.id
                };
