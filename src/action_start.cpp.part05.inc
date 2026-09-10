            need((p.state == "offered" || p.state == "countered" || p.state == "accepted") && p.expires >= state_.time + 1 && ! p.executed,
            "proposal no longer actionable");
            auto answer = str(c.args, "answer");
            need(answer == "accept" || answer == "decline" || answer == "counter", "unknown answer");
            if (answer == "counter") {
                validate_terms(c.actor, p.participants, at(c.args, "terms"));
                auto expiry = get_or < Minute >(c.args, "expires", p.expires);
                need(expiry > state_.time + 1, "counter expires too soon");
            }
            a.proposal = pid;
            a.proposal_version = p.version;
            // Signatures of the same version commute. A concurrent counter invalidates a stale commit.
        } else if (c.type == "A13") {
            auto to = str(c.args, "to"), mode = str(c.args, "mode");
            need(state_.actors.contains(to) && to != c.actor, "invalid receiver");
            if (mode == "gift" || mode == "return" || mode == "loan_item") {
                need(channel_available(c.actor, to, "speech"), "receiver not present/ready");
                auto id = str(c.args, "item");
                reachable(id);
                need(holder(id) == c.actor, "item not held");
                if (mode == "gift" || mode == "loan_item") need(item(id).owner == c.actor, "only owner may gift or establish a loan");
                if (mode == "return") {
                    auto oid = str(c.args, "obligation");
                    need(state_.obligations.contains(oid), "return obligation missing");
                    const auto & o = state_.obligations.at(oid);
                    need(o.kind == "return_item" && o.item == id && o.debtor == c.actor && o.creditor == to &&(o.status == "pending" || o.status == "breached"),
                    "wrong return");
                }
                lock("item/" + id);
                takespace(to, id);
            } else need(mode == "loan_money" || mode == "repay", "unknown transfer mode");
            if (mode == "loan_item" || mode == "loan_money") {
                auto pid = str(c.args, "proposal");
                need(state_.proposals.contains(pid), "missing agreement");
                const auto & p = state_.proposals.at(pid);
                need(p.state == "accepted" && ! p.executed && str(p.terms, "lender") == c.actor && str(p.terms, "borrower") == to,
                "invalid lender/borrower consent");
                need(str(p.terms, "kind") == mode, "agreement mode mismatch");
                need(get < Minute >(p.terms, "due") > state_.time, "loan deadline passed");
                if (mode == "loan_item") need(str(p.terms, "item") == str(c.args, "item"), "wrong loan object");
                else {
                    a.price = get < Money >(p.terms, "amount");
                    need(available(actor.account) >= a.price, "insufficient money");
                    lock("money/" + actor.account, state_.accounts.at(actor.account), a.price);
                }
                a.proposal = pid;
                a.proposal_version = p.version;
                lock("proposal/" + pid);
            } else if (mode == "repay") {
                auto oid = str(c.args, "obligation");
                need(state_.obligations.contains(oid), "unknown debt");
                const auto & o = state_.obligations.at(oid);
                need(o.kind == "repay_money" && o.debtor == c.actor && o.creditor == to &&(o.status == "pending" || o.status == "breached"),
                "wrong debt");
                a.price = get_or < Money >(c.args, "amount", o.remaining);
                need(a.price > 0 && a.price <= o.remaining && available(actor.account) >= a.price, "invalid repayment");
                lock("money/" + actor.account, state_.accounts.at(actor.account), a.price);
                lock("obligation/" + oid);
            }
        } else if (c.type == "A15" || c.type == "A32") {
