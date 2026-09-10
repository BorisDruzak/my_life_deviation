        emit(a.id, type, actors, {
            id
        }, Object {
            {
                "item", id
            }, {
                "holder", holder(id)
            }, {
                "place", item_place(id)
            }, {
                "mode", text(a.args, "mode")
            }, {
                "agreement", a.proposal
            }
        }, false, a.proposal, Object {
            {
                "owner", i.owner
            }, {
                "old_placement", before
            }, {
                "new_placement", encode(where)
            }
        });
    }
    void World::transfer_money(const Id & from, const Id & to, Money amount, const Action & a) {
        if (amount < 0 || from == to || ! state_.accounts.contains(from) || ! state_.accounts.contains(to) || state_.accounts.at(from) < amount || state_.accounts.at(to) > max_money - amount) throw InvariantError("invalid money transaction after validation");
        state_.accounts.at(from) -= amount;
        state_.accounts.at(to) += amount;
        emit(a.id, "money_transferred", a.participants, {}, Object {
            {
                "from", from
            }, {
                "to", to
            }, {
                "amount", amount
            }
        }, true, a.proposal);
    }
    void World::satisfy_obligation(Obligation & o, const Id & cause) {
        if (o.status == "fulfilled" || o.status == "waived" || o.status == "renegotiated") return;
        o.status = "fulfilled";
        o.fulfilled_at = state_.time + 1;
        if (o.kind == "repay_money") o.remaining = 0;
        emit(cause, "obligation_fulfilled", {
            o.debtor
        }, {}, Object {
            {
                "obligation", o.id
            }, {
                "creditor", o.creditor
            }, {
                "debtor", o.debtor
            }, {
                "important", o.important
            }, {
                "late", o.breached_at >= 0
            }
        }, true, o.root);
        if (o.important) {
            Appraisal r;
            r.observer = o.debtor;
            r.root = o.root;
            r.type = "own_success";
            r.esteem_delta = config_.number({
                "emotions", "selfesteem_success_delta"
            });
            pending_appraisals_.push_back(r);
        }
        o.acknowledged.insert(o.debtor);
    }
    void World::transfer_action(const Action & a) {
        auto mode = str(a.args, "mode"), to = str(a.args, "to");
        if (mode == "gift" || mode == "loan_item" || mode == "return") {
            auto item = str(a.args, "item");
            move_item(item, {
                "inventory", to
            }, mode == "gift" ? to : Id {}, a, "item_transferred");
        }
        if (mode == "loan_money" || mode == "repay") transfer_money(state_.actors.at(a.actor).account, state_.actors.at(to).account,
        a.price, a);
        if (mode == "loan_item" || mode == "loan_money") {
            auto & p = state_.proposals.at(a.proposal);
            p.executed = true;
            Obligation o;
            o.id = p.id + "/obligation";
            o.root = p.root;
            o.debtor = to;
            o.creditor = a.actor;
            o.kind = mode == "loan_item" ? "return_item" : "repay_money";
            o.item = text(a.args, "item");
            o.amount = a.price;
            o.remaining = a.price;
            o.due = get < Minute >(p.terms, "due");
            o.important = get_or < bool >(p.terms, "important", false);
            o.acknowledged = {
                a.actor, to
            };
            state_.obligations.emplace(o.id, o);
            emit(a.id, "obligation_created", {
                a.actor, to
            }, {}, encode(o), true, o.root);
            appraise(to, a.actor, o.root, "help");
        } else if (mode == "return") {
            auto & o = state_.obligations.at(str(a.args, "obligation"));
            satisfy_obligation(o, a.id);
            o.acknowledged.insert(to);
