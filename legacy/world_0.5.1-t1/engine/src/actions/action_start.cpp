#include "npc/world.hpp"
#include "npc/math.hpp"
#include <algorithm>
namespace npc {
    namespace {
        void need(bool ok, const char * why) {
            if (! ok) throw InputError(why);
        }
        Minute duration(const Json & j, Minute def, Minute maximum) {
            auto d = get_or < Minute >(j, "minutes", def);
            need(d >= 1 && d <= maximum, "duration out of range");
            return d;
        }
        bool portable(const Json & t) {
            return has(t, "mass_kg") && ! has(t, "slots");
        }
        void unique_ids(const std::vector < Id > & v) {
            std::set < Id > s(v.begin(), v.end());
            need(s.size() == v.size(), "duplicate object binding");
        }
        const std::map < std::string, std::set < std::string >> fields = {
            {
                "A01", {
                    "minutes"
                }
            }, {
                "A02", {
                    "edge"
                }
            }, {
                "A03", {
                    "item", "place", "scope"
                }
            }, {
                "A04", {
                    "item"
                }
            }, {
                "A05", {
                    "item", "container"
                }
            }, {
                "A06", {
                    "item", "shop"
                }
            }, {
                "A07", {
                    "item"
                }
            }, {
                "A08", {
                    "job", "recipe", "fixture", "materials"
                }
            }, {
                "A09", {
                    "bed", "minutes"
                }
            }, {
                "A10", {
                    "contract", "minutes"
                }
            }, {
                "A11", {
                    "to", "terms", "expires", "channel"
                }
            }, {
                "A12", {
                    "proposal", "version", "answer", "terms", "expires", "channel"
                }
            }, {
                "A13", {
                    "mode", "to", "item", "proposal", "obligation", "amount"
                }
            }, {
                "A14", {
                    "participants", "terms", "expires", "channel"
                }
            }, {
                "A15", {
                    "proposal", "version"
                }
            }, {
                "A16", {
                    "item", "minutes"
                }
            }, {
                "A17", {
                    "to", "operator", "evidence", "channel"
                }
            }, {
                "A18", {
                    "to", "question", "mode", "statement", "evidence", "channel", "fabricated"
                }
            }, {
                "A19", {
                    "to", "statement", "evidence", "channel", "fabricated"
                }
            }, {
                "A20", {
                    "item"
                }
            }, {
                "A21", {
                    "job", "recipe", "target", "tools", "materials"
                }
            }, {
                "A22", {
                    "proposal"
                }
            }, {
                "A23", {
                    "minutes"
                }
            }, {
                "A24", {
                    "message", "record", "version"
                }
            }, {
                "A25", {
                    "resource", "version", "grantee", "grant"
                }
            }, {
                "A26", {
                    "item", "open"
                }
            }, {
                "A27", {
                    "to", "obligation", "episode", "forceful", "channel"
                }
            }, {
                "A28", {
                    "statement", "evidence", "acl", "public", "fabricated"
                }
            }, {
                "A29", {
                    "record", "version", "delete", "statement", "acl", "public"
                }
            }, {
                "A30", {
                    "skill", "tool", "minutes"
                }
            }, {
                "A31", {}
            }, {
                "A32", {
                    "proposal", "version"
                }
            }
        };
    }
    bool World::try_start(const Command & c, const std::vector < Command > & batch) {
        auto & actor = state_.actors.at(c.actor);
        need(actor.alive && actor.capable, "actor incapacitated");
        need(actor.active_action.empty(), "actor busy");
        need(actor.position.kind == "at" ||(c.type == "A02" && actor.position.kind == "stranded"), "actor in transit");
        for (const auto & kv : c.args.as_object()) need(fields.at(c.type).contains(std::string(kv.key())), "unknown action parameter");
        for (const auto &[id, version] : c.expected_versions) {
            if (state_.items.contains(id)) need(state_.items.at(id).version == version, "object version changed");
            else if (state_.permissions.contains(id)) need(state_.permissions.at(id).version == version, "ACL version changed");
            else throw InputError("unknown versioned object");
        }
        Action a;
        a.id = command_key(c);
        a.type = c.type;
        a.actor = c.actor;
        a.participants = {
            c.actor
        };
        a.commands = {
            a.id
        };
        a.args = c.args;
        a.started = state_.time;
        a.intent = c.intent;
        Minute d = 1;
        std::vector < Reservation > locks;
        std::optional < Job > new_job;
        auto lock =[&](std::string key, Money capacity = 1, Money amount = 1) {
            if (amount == 0) return;
            Reservation r;
            r.resource = std::move(key);
            r.action = a.id;
            r.capacity = capacity;
            r.amount = amount;
            locks.push_back(r);
        };
        auto item =[&](const Id & id)->const Item & {
            auto it = state_.items.find(id);
            need(it != state_.items.end(), "unknown/unavailable item");
            return it->second;
        };
        auto reachable =[&](const Id & id) {
            need(accessible(c.actor, id), "item not physically accessible");
        };
        auto allowed =[&](const Id & id) {
            reachable(id);
            need(permitted(c.actor, id), "no permission");
        };
        auto takespace =[&](const Id & who, const Id & id) {
            auto add = holder(id) == who ? 0 : item_mass(id);
            need(carried_mass(who) + add <= state_.actors.at(who).capacity_kg + 1e-9, "carry limit");
            lock("inventory/" + who);
        };
        auto phone =[&]() {
            for (const auto &[id, x] : state_.items) if (x.type == "I10" && holder(id) == c.actor && accessible(c.actor, id)) return true;
            return false;
        };
        auto contact =[&](const Id & to) {
            need(channel_available(c.actor, to, text(c.args, "channel", "speech")), "contact unavailable");
        };
        auto known_evidence =[&]() {
            auto refs = get_or < std::vector < Id >>(c.args, "evidence", {});
            for (const auto & id : refs) need(std::any_of(actor.beliefs.begin(), actor.beliefs.end(),[&](const auto & e) {
                return e.id == id;
            }), "unknown evidence reference");
            return refs;
        };
        auto claim =[&]() {
            need(has(c.args, "statement"), "missing statement");
            auto s = decode < Statement >(at(c.args, "statement"));
            need(! s.subject.empty() && ! s.predicate.empty() && s.valid_from >= 0, "invalid statement");
            auto refs = known_evidence();
            need(! refs.empty() || get_or < bool >(c.args, "fabricated", false), "unfounded statement must be explicitly marked for private audit");
        };
        if (c.type == "A01" || c.type == "A23") {
            d = duration(c.args, 1, 120);
            need(location_access(c.actor, actor.position.place), "place unavailable");
        } else if (c.type == "A02") {
            auto eid = str(c.args, "edge");
            need(state_.edges.contains(eid), "unknown edge");
            const auto & e = state_.edges.at(eid);
            need(e.open, "edge closed");
            double progress = 0;
            if (actor.position.kind == "stranded") {
                need(actor.position.edge == eid, "must resume the same edge");
                progress = actor.position.progress;
            } else need(actor.position.place == e.from, "not at edge origin");
            need(location_access(c.actor, e.to), "destination access denied");
            if (! e.key.empty()) need(holder(e.key) == c.actor, "key required");
            need(carried_mass(c.actor) <= actor.capacity_kg + 1e-9, "carry limit");
            double speed = 1;
            if (config_.enabled(state_.profile, "variable_movement_speed")) speed = std::max(config_.number({
                "physical", "movement_min_speed"
            }), actor.base_speed *(1 - config_.number({
                "physical", "movement_fatigue_factor"
            }) * actor.fatigue) *(config_.number({
                "physical", "movement_health_base"
            }) + config_.number({
                "physical", "movement_health_factor"
            }) * actor.health));
            d = std::max < Minute >(1, static_cast < Minute >(std::ceil(e.minutes *(1 - progress) / speed)));
            a.captured = Object {
                {
                    "base_progress", progress
                }, {
                    "edge", eid
                }
            };
        } else if (c.type == "A03") {
            need(text(c.args, "scope", "all") == "all" || text(c.args, "scope") == "stock", "unknown observation scope");
            d = 2;
            if (has(c.args, "item")) {
                reachable(str(c.args, "item"));
            } else need(at_place(c.actor, str(c.args, "place")), "place not visible");
        } else if (c.type == "A04" || c.type == "A20") {
            auto id = str(c.args, "item");
            reachable(id);
            need(holder(id) != c.actor, "item already held; no transfer occurs");
            need(portable(config_.item_type(item(id).type)), "object fixed in place");
            if (c.type == "A04") need(permitted(c.actor, id), "permission required");
            else need(! permitted(c.actor, id), "use the permitted transfer action");
            takespace(c.actor, id);
            lock("item/" + id);
            a.captured = Object {
                {
                    "owner", item(id).owner
                }, {
                    "placement", encode(item(id).placement)
                }
            };
        } else if (c.type == "A05") {
            auto id = str(c.args, "item");
            reachable(id);
            need(holder(id) == c.actor, "not carrying the item");
            lock("item/" + id);
            lock("inventory/" + c.actor);
            if (has(c.args, "container")) {
                auto dst = str(c.args, "container");
                allowed(dst);
                const auto & box = item(dst);
                const auto & t = config_.item_type(box.type);
                need(box.open && has(t, "capacity_kg"), "container closed or unsuitable");
                auto parent = dst;
                std::set < Id > seen;
                while (state_.items.contains(parent) && seen.insert(parent).second) {
                    need(parent != id, "containment cycle");
                    const auto & p = state_.items.at(parent).placement;
                    if (p.kind == "in" || p.kind == "installed") parent = p.ref;
                    else break;
                }
                double contained = item_mass(dst) - get_or < double >(t, "mass_kg", 0);
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
            need(actor.known_methods.contains(j.recipe), "recipe not known");
            const auto & recipe = config_.recipes.at(j.recipe);
            auto sk = actor.skills.find(j.recipe == "R01" ? "cooking" : "repair");
            need((sk == actor.skills.end() ? 0 : sk->second) >= get < double >(recipe, "min_skill"), "skill insufficient");
            if (j.recipe == "R01") {
                allowed(j.fixture);
                need(item(j.fixture).type == "F02", "kitchen required");
                lock("item/" + j.fixture);
            } else {
                need(actor.heavy_allowed, "heavy work temporarily blocked");
                allowed(j.target);
                need(item(j.target).type == text(recipe, "target"), "wrong technical target");
                lock("item/" + j.target);
            }
            for (auto & id : j.tools) {
                allowed(id);
                const auto & i = item(id);
                const auto & t = config_.item_type(i.type);
                if (j.recipe == "R02") need(i.type == "I05" && i.condition >= get_or < double >(t, "required_condition", .5), "unsuitable repair tool");
                if (j.recipe == "R03") need(get_or < double >(t, "length_m", 0) >= 3 && get_or < double >(t, "strength_game_units",
                0) >= 20, "fastener properties insufficient");
                lock("item/" + id);
            }
            for (auto & id : j.materials) {
                if (! new_job) need(item(id).placement.kind == "escrow" && item(id).placement.ref == j.id, "lost job material");
                lock("item/" + id);
            }
            a.job = j.id;
            d = j.required - j.worked;
            need(d > 0, "job already completed");
        } else if (c.type == "A09") {
            d = duration(c.args, 60, 720);
            auto id = str(c.args, "bed");
            allowed(id);
            need(item(id).type == "F01", "sleep place required");
            lock("item/" + id);
        } else if (c.type == "A10") {
            need(actor.heavy_allowed, "heavy work blocked");
            d = duration(c.args, 60, 480);
            auto id = str(c.args, "contract");
            need(state_.contracts.contains(id), "employment unavailable");
            const auto & ct = state_.contracts.at(id);
            need(ct.worker == c.actor, "not your contract");
            allowed(ct.workplace);
            for (Minute t = state_.time; t < state_.time + d; ++ t) {
                bool okay = false;
                for (auto & w : ct.windows) if (w[0] <= t % 1440 && t % 1440 < w[1]) okay = true;
                need(okay, "outside work schedule");
            }
            lock("item/" + ct.workplace);
            lock("contract/" + id);
        } else if (c.type == "A11" || c.type == "A14") {
            std::set < Id > participants;
            if (c.type == "A11") participants = {
                c.actor, str(c.args, "to")
            };
            else {
                participants = get < std::set < Id >>(c.args, "participants");
                participants.insert(c.actor);
            }
            need(participants.size() >= 2 && participants.size() <= 16, "invalid participant count");
            for (auto & who : participants) if (who != c.actor) contact(who);
            auto expires = get_or < Minute >(c.args, "expires", state_.time + 120);
            need(expires > state_.time + 1 && expires <= state_.time + 10080, "invalid expiry");
            validate_terms(c.actor, participants, at(c.args, "terms"));
        } else if (c.type == "A12") {
            auto pid = str(c.args, "proposal");
            need(state_.proposals.contains(pid), "unknown proposal");
            const auto & p = state_.proposals.at(pid);
            need(p.participants.contains(c.actor) && p.version == get < std::int64_t >(c.args, "version") && known_proposal(c.actor,
            pid, p.version), "proposal version not received");
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
            auto pid = str(c.args, "proposal");
            need(state_.proposals.contains(pid), "unknown agreement");
            const auto & p = state_.proposals.at(pid);
            need(p.state == "accepted" && ! p.executed && p.signatures == p.participants && p.version == get < std::int64_t >(c.args,
            "version"), "no signed current agreement");
            need(p.participants.contains(c.actor), "not a participant");
            auto kind = str(p.terms, "kind");
            need((c.type == "A32" && kind == "private") ||(c.type == "A15" && kind == "joint"), "wrong joint kind");
            d = get < Minute >(p.terms, "minutes");
            auto place = str(p.terms, "place");
            need(state_.time >= get < Minute >(p.terms, "start") && state_.time + d <= get < Minute >(p.terms, "end"), "outside signed window");
            a.participants.assign(p.participants.begin(), p.participants.end());
            a.commands.clear();
            for (auto & who : a.participants) {
                need(at_place(who, place) && awake(who) && state_.actors.at(who).active_action.empty() && location_access(who, place),
                "participant unavailable");
                auto ready = std::find_if(batch.begin(), batch.end(),[&](const Command & q) {
                    return q.actor == who && q.type == c.type && text(q.args, "proposal") == pid && get_or < std::int64_t >(q.args,
                    "version", 0) == p.version && state_.receipts.at(command_key(q)).status == "queued";
                });
                need(ready != batch.end(), "missing independently submitted current readiness");
                a.commands.push_back(command_key(* ready));
                if (c.type == "A32") need(config_.enabled(state_.profile, "intimacy") && state_.actors.at(who).needs.contains("N05") && state_.actors.at(who).needs.at("N05").enabled && state_.time / 1440 - state_.actors.at(who).birth_day >= 365 * 18,
                "adult module not available");
            }
            if (c.type == "A32") {
                need(a.participants.size() == 2 && state_.places.at(place).private_space, "private room required");
                for (auto &[who, other] : state_.actors) if (! p.participants.contains(who)) need(! at_place(who, place) || ! other.alive,
                "privacy interrupted");
            }
            a.proposal = pid;
            a.proposal_version = p.version;
            a.captured = p.terms;
            lock("place/" + place, state_.places.at(place).capacity, static_cast < Money >(a.participants.size()));
            lock("proposal/" + pid);
        } else if (c.type == "A16") {
            d = duration(c.args, 30, 120);
            auto id = str(c.args, "item");
            allowed(id);
            const auto & t = config_.item_type(item(id).type);
            need(has(t, "pleasure_per_30_min"), "no leisure affordance");
            auto topic = str(t, "category");
            need(actor.known_topics.contains(topic), "activity not known");
            if (topic == "sport") need(actor.heavy_allowed, "sport blocked");
            lock("item/" + id);
        } else if (c.type == "A17") {
            contact(str(c.args, "to"));
            auto op = str(c.args, "operator");
            need(std::set < std::string > {
                "WHO", "WHAT", "WHERE", "WHEN", "WHY", "CONFIRM", "COMPARE"
            }
            .contains(op), "invalid question operator");
            auto refs = known_evidence();
            need(! refs.empty() &&(op != "COMPARE" || refs.size() == 2), "question needs evidence");
        } else if (c.type == "A18") {
            contact(str(c.args, "to"));
            auto q = str(c.args, "question");
            need(state_.messages.contains(q) && state_.messages.at(q).readers.contains(c.actor) && text(state_.messages.at(q).content,
            "type") == "question", "question not received/read");
            need(state_.messages.at(q).author == str(c.args, "to"), "wrong question author");
            auto mode = text(c.args, "mode", "truth");
            need(mode == "truth" || mode == "partial" || mode == "refuse" || mode == "lie", "unknown answer mode");
            if (mode != "refuse") claim();
        } else if (c.type == "A19") {
            contact(str(c.args, "to"));
            claim();
        } else if (c.type == "A24") {
            if (has(c.args, "message")) {
                auto id = str(c.args, "message");
                need(state_.messages.contains(id), "message unavailable");
                const auto & m = state_.messages.at(id);
                need(m.delivered && std::find(m.recipients.begin(), m.recipients.end(), c.actor) != m.recipients.end(), "not delivered to this reader");
                if (m.channel == "digital") need(phone(), "digital interface unavailable");
                a.captured = m.content;
            } else {
                auto id = str(c.args, "record");
                need(state_.records.contains(id), "record unavailable");
                const auto & r = state_.records.at(id);
                const auto & v = r.versions.back();
                need(record_access(c.actor, v), "record access denied");
                need(get_or < std::int64_t >(c.args, "version", v.version) == v.version, "record version changed");
                need(phone(), "digital interface unavailable");
                a.captured = encode(v);
            }
        } else if (c.type == "A25") {
            auto id = str(c.args, "resource"), who = str(c.args, "grantee");
            need(state_.actors.contains(who), "unknown grantee");
            need(state_.items.contains(id) || state_.places.contains(id), "unsupported ACL target");
            auto authority = state_.permissions.contains(id) ? state_.permissions.at(id).authority : state_.items.contains(id) ? state_.items.at(id).owner : state_.places.at(id).owner;
            auto v = state_.permissions.contains(id) ? state_.permissions.at(id).version : 1;
            need(authority == c.actor && get < std::int64_t >(c.args, "version") == v, "ACL authority/version mismatch");
            static_cast < void >(get < bool >(c.args, "grant"));
            lock("acl/" + id);
        } else if (c.type == "A26") {
            auto id = str(c.args, "item");
            allowed(id);
            const auto & i = item(id);
            need(has(config_.item_type(i.type), "capacity_kg"), "not a container");
            if (i.locked) need(! i.key.empty() && holder(i.key) == c.actor, "key required");
            static_cast < void >(get < bool >(c.args, "open"));
            lock("item/" + id);
        } else if (c.type == "A27") {
            auto to = str(c.args, "to"), oid = str(c.args, "obligation");
            contact(to);
            need(state_.obligations.contains(oid), "unknown obligation");
            const auto & o = state_.obligations.at(oid);
            need(o.creditor == c.actor && o.debtor == to, "not the creditor");
            need(! text(c.args, "episode").empty(), "conversation episode required");
        } else if (c.type == "A28") {
            d = 2;
            need(phone(), "publication interface unavailable");
            claim();
            for (auto & who : get_or < std::set < Id >>(c.args, "acl", {})) need(state_.actors.contains(who), "unknown reader");
        } else if (c.type == "A29") {
            auto id = str(c.args, "record");
            need(state_.records.contains(id), "record unavailable");
            const auto & r = state_.records.at(id);
            need(r.author == c.actor && r.versions.back().version == get < std::int64_t >(c.args, "version"), "not author or stale version");
            need(phone(), "publication interface unavailable");
            for (auto & who : get_or < std::set < Id >>(c.args, "acl", {})) need(state_.actors.contains(who), "unknown reader");
            if (! get_or < bool >(c.args, "delete", false)) static_cast < void >(decode < Statement >(at(c.args, "statement")));
            lock("record/" + id);
        } else if (c.type == "A30") {
            need(config_.enabled(state_.profile, "skill_learning"), "learning profile disabled");
            d = duration(c.args, 30, 120);
            auto skill = str(c.args, "skill"), tool = str(c.args, "tool");
            need(skill == "repair" || skill == "cooking", "unknown practice contract");
            allowed(tool);
            need((skill == "repair" && item(tool).type == "I05") ||(skill == "cooking" && item(tool).type == "F02"), "wrong practice tool");
            lock("item/" + tool);
        } else if (c.type == "A31") {
            d = 10;
            need(config_.enabled(state_.profile, "intimacy") && actor.needs.contains("N05") && actor.needs.at("N05").enabled && state_.time / 1440 - actor.birth_day >= 365 * 18,
            "adult module disabled");
            need(state_.places.at(actor.position.place).private_space && location_access(c.actor, actor.position.place), "private place required");
            for (auto &[who, x] : state_.actors) if (who != c.actor) need(! at_place(who, actor.position.place) || ! x.alive,
            "privacy interrupted");
        } else throw InputError("unimplemented action type");
        need(d > 0 && d <= 14400 && state_.time <= max_minute - d, "invalid fixed duration");
        a.due = state_.time + d;
        for (auto & who : a.participants) lock("actor/" + who);
        need(lockable(locks), "required resources busy");
        // All checks precede changes. From here on there are no expected input failures.
        state_.actions.emplace(a.id, a);
        for (std::size_t i = 0; i < locks.size(); ++ i) {
            auto r = locks[i];
            r.id = a.id + "/lock/" + std::to_string(i);
            r.until = a.due;
            state_.reservations.emplace(r.id, r);
        }
        for (auto & who : a.participants) state_.actors.at(who).active_action = a.id;
        for (auto & key : a.commands) {
            auto & r = state_.receipts.at(key);
            r.status = "started";
            r.action = a.id;
        }
        // The departure event captures the origin geometry, before changing position.
        emit(a.id, c.type == "A02" ? "departed" : c.type == "A09" ? "sleep_started" : c.type == "A31" || c.type == "A32" ? "private_activity_started" : c.type == "A15" ? "joint_started" : "action_started",
        a.participants, {}, Object {
            {
                "action_type", c.type
            }
        }, c.type == "A31" || c.type == "A32", {}, Object {
            {
                "intent", c.intent
            }
        });
        if (c.type == "A02") {
            const auto & e = state_.edges.at(str(c.args, "edge"));
            Position p;
            p.kind = "transit";
            p.edge = e.id;
            p.from = e.from;
            p.to = e.to;
            p.started = state_.time;
            p.due = a.due;
            p.base_progress = get < double >(a.captured, "base_progress");
            p.progress = p.base_progress;
            actor.position = p;
        }
        if (! a.job.empty()) {
            if (new_job) {
                auto j = * new_job;
                j.active_action = a.id;
                j.status = "active";
                state_.jobs[j.id] = j;
                for (auto & x : j.materials) {
                    auto & i = state_.items.at(x);
                    i.placement = {
                        "escrow", j.id
                    };
                    ++ i.version;
                }
                emit(a.id, "job_started", {
                    c.actor
                }, j.materials, Object {
                    {
                        "job", j.id
                    }, {
                        "recipe", j.recipe
                    }
                }, false, j.id);
            } else {
                auto & j = state_.jobs.at(a.job);
                j.active_action = a.id;
                j.status = "active";
            }
        }
        if (c.type == "A15" || c.type == "A32") state_.proposals.at(a.proposal).executed = true;
        return true;
    }
}
