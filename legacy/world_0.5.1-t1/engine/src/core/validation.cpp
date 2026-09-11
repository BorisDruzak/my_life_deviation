#include "npc/config.hpp"
#include <functional>
#include <numeric>
namespace npc {
    namespace {
        void require(bool b, const std::string & message) {
            if (! b) throw InputError(message);
        }
        void ratio(double x, const std::string & name, double upper = 1) {
            require(std::isfinite(x) && x >= 0 && x <= upper, "invalid " + name);
        }
        void windows(const std::vector < std::vector < Minute >> & v) {
            require(! v.empty(), "empty opening schedule");
            Minute end = - 1;
            for (auto & w : v) {
                require(w.size() == 2 && 0 <= w[0] && w[0] < w[1] && w[1] <= 1440 && w[0] >= end, "invalid/overlapping opening window");
                end = w[1];
            }
        }
        void ident(const std::string & id) {
            require(! id.empty() && id.size() <= 256, "invalid ID");
            for (unsigned char c : id) require(c >= 32 && c != 127, "control character in ID");
        }
    }
    void validate_state(const Config & c, const State & s, bool restored) {
        require(s.version == rules_version, "state rules version mismatch");
        require(s.config_hash == c.hash, "state config hash mismatch");
        require(s.profile == "baseline" || s.profile == "extended", "unknown profile");
        require(s.time >= 0 && s.time < max_minute, "invalid world time");
        require(! s.actors.empty() && ! s.places.empty(), "world needs actors and places");
        require(s.actors.size() <= 10000, "actor limit exceeded");
        Money sum = 0;
        for (const auto &[id, b] : s.accounts) {
            ident(id);
            require(b >= 0 && b <= max_money && sum <= max_money - b, "invalid money balance/overflow");
            sum += b;
        }
        require(s.genesis_money >= 0 && s.genesis_money <= max_money, "invalid genesis money");
        require(s.external_money >= - max_money && s.external_money <= max_money, "invalid external money");
        require(sum == s.genesis_money + s.external_money, "money conservation violated");
        for (const auto &[id, p] : s.places) {
            ident(id);
            require(p.id == id, "place ID mismatch");
            require(p.capacity > 0, "place capacity must be positive");
            windows(p.open_windows);
        }
        for (const auto &[id, e] : s.edges) {
            ident(id);
            require(e.id == id && s.places.contains(e.from) && s.places.contains(e.to), "invalid edge");
            require(e.minutes >= 1 && e.minutes <= 14400, "invalid edge duration");
            if (! e.key.empty()) require(s.items.contains(e.key), "unknown edge key");
        }
        for (const auto &[id, a] : s.actors) {
            ident(id);
            require(id.find('@') == std::string::npos && id.find('/') == std::string::npos, "actor ID must not contain @ or /");
            require(a.id == id, "actor ID mismatch");
            require(s.accounts.contains(a.account), "missing actor account");
            require(a.birth_day >= - 365000 && a.birth_day <= s.time / 1440, "invalid birth day");
            require(std::isfinite(a.capacity_kg) && a.capacity_kg > 0 && a.capacity_kg <= 100000, "invalid carry capacity");
            require(std::isfinite(a.base_speed) && a.base_speed > 0 && a.base_speed <= 1000, "invalid movement speed");
            for (auto x : {
                a.health, a.fatigue, a.pain, a.impairment, a.stress, a.fear, a.selfesteem, a.selfesteem_base, a.attention
            }) ratio(x, "actor state");
            require(std::isfinite(a.wellbeing) && a.wellbeing >= 0 && a.wellbeing <= static_cast < double >(max_money), "invalid wellbeing");
            require(a.horizon >= 1 && a.horizon <= 10080 && a.search_width >= 1 && a.search_width <= 128, "invalid cognitive budget");
            for (auto key : {
                "patience", "caution", "social_initiative", "confrontation", "novelty"
            }) require(a.traits.contains(key), "missing trait");
            for (const auto &[k, v] : a.traits) ratio(v, "trait");
            for (const auto &[k, v] : a.norm_prices) ratio(v, "norm price");
            for (auto name : {
                "N01", "N02", "N03", "N04"
            }) require(a.needs.contains(name) && a.needs.at(name).enabled, "missing baseline need");
            for (const auto &[n, v] : a.needs) {
                require(has(at(c.parameters, "needs"), n), "unknown need");
                ratio(v.value, "need", 100);
                require(0 < v.critical && v.critical < v.activation && v.activation < v.target && v.target <= 100, "invalid need thresholds");
                for (auto d : {
                    v.awake_drain, v.sleep_drain, v.sleep_gain
                }) require(std::isfinite(d) && d >= 0 && d <= 1000, "invalid need rate");
                if (n == "N05" && v.enabled) require(c.enabled(s.profile, "intimacy") &&(s.time / 1440 - a.birth_day) >= 365 * 18,
                "adult module disabled or minor");
            }
            for (const auto &[k, v] : a.interests) ratio(v, "interest");
            for (const auto &[k, v] : a.skills) ratio(v, "skill");
            for (const auto &[k, v] : a.anger) {
                require(s.actors.contains(k), "unknown anger target");
                ratio(v, "anger");
            }
            for (const auto &[k, v] : a.repetition) require(std::isfinite(v) && v >= 0, "invalid repetition");
            for (const auto &[k, v] : a.drives) {
                ratio(v.intensity, "drive intensity");
                ratio(v.pressure, "drive pressure");
                require(v.intensity != 0 || v.pressure == 0, "disabled drive has pressure");
                require(k == "appropriation", "unregistered drive");
            }
            for (const auto &[other, r] : a.relations) {
                require(other != id && s.actors.contains(other), "invalid relationship target");
                for (auto v : {
                    r.affection, r.trust, r.tension, r.attraction
                }) ratio(v, "relationship");
            }
            const auto & p = a.position;
            if (p.kind == "at") require(s.places.contains(p.place) && p.edge.empty(), "invalid at position");
            else if (p.kind == "transit" || p.kind == "stranded") {
                require(p.place.empty() && s.edges.contains(p.edge), "invalid transit position");
                const auto & e = s.edges.at(p.edge);
                require(p.from == e.from && p.to == e.to, "edge endpoints mismatch");
                ratio(p.progress, "edge progress");
                ratio(p.base_progress, "base progress");
                if (p.kind == "transit") require(p.started <= s.time && p.due > s.time && ! a.active_action.empty(), "transit without active movement");
            } else throw InputError("unknown position kind");
            if (! a.active_action.empty()) require(s.actions.contains(a.active_action), "actor references missing active action");
            if (restored) {
                require((a.health > 0) == a.alive, "terminal flag mismatch");
                if (! a.alive) require(a.active_action.empty(), "dead actor active");
            }
            std::set < Id > ids;
            for (const auto & b : a.beliefs) {
                ident(b.id);
                require(ids.insert(b.id).second, "duplicate evidence ID");
                ratio(b.confidence, "belief confidence");
                ratio(b.prior, "belief prior");
                ratio(b.severity, "belief severity");
                require(std::isfinite(b.half_life) && b.half_life >= 0, "invalid belief half life");
                require(b.learned_at <= s.time && b.learned_at >= 0, "belief from future");
                require(! b.roots.empty(), "evidence without provenance");
            }
            for (const auto & m : a.inbox) require(s.messages.contains(m), "missing inbox message");
        }
        std::function < double(const Id &, std::set < Id > &) > mass =[&](const Id & id, std::set < Id > & seen)->double {
            require(s.items.contains(id), "missing object");
            require(seen.insert(id).second, "container cycle");
            const auto & i = s.items.at(id);
            const auto & t = c.item_type(i.type);
            double m = get_or < double >(t, "mass_kg", 0);
            if (i.placement.kind == "tombstone") m = 0;
            else if (i.total_units > 0) m *= static_cast < double >(i.remaining_units) / static_cast < double >(i.total_units);
            double contents = 0;
            for (const auto &[x, y] : s.items) if ((y.placement.kind == "in" || y.placement.kind == "installed") && y.placement.ref == id) contents += mass(x,
            seen);
            if (has(t, "capacity_kg")) require(contents <= get < double >(t, "capacity_kg") + 1e-9, "container over capacity");
            seen.erase(id);
            return m + contents;
        };
        for (const auto &[id, i] : s.items) {
            ident(id);
            require(i.id == id, "item ID mismatch");
            const auto & t = c.item_type(i.type);
            ratio(i.condition, "condition");
            require(i.version >= 1, "invalid object version");
            require(i.total_units >= 0 && i.remaining_units >= 0 && i.remaining_units <= i.total_units, "invalid portion");
            if (get_or < bool >(t, "edible", false)) require(i.total_units == get < std::int64_t >(t, "total_units"), "portion total changed");
            const auto & p = i.placement;
            if (p.kind == "inventory") require(s.actors.contains(p.ref), "invalid inventory");
            else if (p.kind == "at") require(s.places.contains(p.ref), "invalid item place");
            else if (p.kind == "in" || p.kind == "installed") {
                require(s.items.contains(p.ref) && p.ref != id, "invalid containment");
                if (p.kind == "in") require(has(c.item_type(s.items.at(p.ref).type), "capacity_kg"), "parent is not a container");
            } else if (p.kind == "escrow") require(s.jobs.contains(p.ref), "missing job escrow");
            else require(p.kind == "tombstone", "unknown placement kind");
            if (p.kind != "tombstone") {
                std::set < Id > seen;
                static_cast < void >(mass(id, seen));
            }
        }
        for (const auto &[id, a] : s.actors) {
            double load = 0;
            for (const auto &[k, i] : s.items) if (i.placement.kind == "inventory" && i.placement.ref == id) {
                std::set < Id > seen;
                load += mass(k, seen);
            }
            require(load <= a.capacity_kg + 1e-9, "actor over carry capacity");
        }
        for (const auto &[id, sh] : s.shops) {
            require(sh.id == id && s.places.contains(sh.place) && s.accounts.contains(sh.account) && sh.slots > 0, "invalid shop");
            for (const auto &[item, price] : sh.stock) {
                require(s.items.contains(item), "missing stock item");
                require(price >= 0 && price <= max_money, "invalid price");
            }
        }
        for (const auto &[id, p] : s.permissions) {
            require(p.resource == id && p.version >= 1 && ! p.authority.empty(), "invalid permission");
            require(s.items.contains(id) || s.places.contains(id) || s.records.contains(id), "missing ACL resource");
            for (const auto & who : p.grantees) {
                require(s.actors.contains(who), "unknown grantee");
                require(! p.denied.contains(who), "contradictory ACL");
            }
            for (const auto & who : p.denied) require(s.actors.contains(who), "unknown denied actor");
        }
        for (const auto &[id, ct] : s.contracts) {
            require(ct.id == id && s.actors.contains(ct.worker) && s.accounts.contains(ct.employer_account) && s.items.contains(ct.workplace),
            "invalid employment");
            require(ct.rate_per_hour >= 0 && ct.rate_per_hour <= max_money / 1440 && ct.accrued_numerator >= 0 && ct.accrued_numerator <= max_money,
            "invalid accrued wage");
            require(ct.next_payout >= 0 && ct.payout_period > 0, "invalid payout schedule");
            windows(ct.windows);
        }
        std::map < Id, Money > reserved;
        for (const auto &[id, r] : s.reservations) {
            require(id == r.id && s.actions.contains(r.action), "orphan reservation");
            require(r.amount > 0 && r.capacity >= r.amount && r.until > s.time, "invalid reservation");
            require(reserved[r.resource] <= max_money - r.amount, "reservation overflow");
            reserved[r.resource] += r.amount;
            require(reserved[r.resource] <= r.capacity, "overlapping exclusive reservation");
        }
        for (const auto &[id, a] : s.actions) {
            require(id == a.id && c.actions.contains(a.type) && a.started <= s.time && a.due > s.time, "invalid active action");
            require(a.elapsed >= 0 && a.elapsed == s.time - a.started, "action progress clock mismatch");
            require(! a.participants.empty(), "action without actor");
            for (const auto & who : a.participants) {
                require(s.actors.contains(who) && s.actors.at(who).active_action == id, "action/actor slot mismatch");
            }
            for (const auto & k : a.commands) require(s.receipts.contains(k), "missing command receipt");
            if (! a.job.empty()) require(s.jobs.contains(a.job), "missing job");
        }
        for (const auto &[id, j] : s.jobs) {
            require(id == j.id && c.recipes.contains(j.recipe) && s.places.contains(j.place) && s.actors.contains(j.creator),
            "invalid job");
            require(j.worked >= 0 && j.worked <= j.required && j.required > 0, "invalid job progress");
            for (auto & x : j.materials) require(s.items.contains(x), "missing job material");
            if (! j.active_action.empty()) require(s.actions.contains(j.active_action), "missing job action");
            if (j.status == "completed") require(j.worked == j.required, "unfinished completed job");
        }
        for (const auto &[id, p] : s.proposals) {
            require(id == p.id && p.version > 0 && p.participants.contains(p.author) && p.expires >= p.created, "invalid proposal");
            for (auto & who : p.participants) require(s.actors.contains(who), "unknown participant");
            for (auto & who : p.signatures) require(p.participants.contains(who), "foreign signature");
            if (p.state == "accepted") require(p.signatures == p.participants, "accepted without all signatures");
        }
        for (const auto &[id, o] : s.obligations) {
            require(id == o.id && s.actors.contains(o.debtor) && s.actors.contains(o.creditor) && o.debtor != o.creditor, "invalid obligation");
            require(o.amount >= 0 && o.remaining >= 0 && o.remaining <= o.amount, "invalid debt");
            require(o.due >= 0, "invalid obligation deadline");
        }
        for (const auto &[id, r] : s.records) {
            require(id == r.id && s.actors.contains(r.author) && ! r.versions.empty(), "invalid record");
            std::int64_t v = 0;
            for (const auto & rv : r.versions) {
                require(rv.version == ++ v && rv.time <= s.time, "record version gap");
                for (const auto & who : rv.acl) require(s.actors.contains(who), "unknown record ACL reader");
            }
        }
        for (const auto &[id, m] : s.messages) {
            require(id == m.id && s.actors.contains(m.author) && m.sent <= s.time && m.deliver_at >= m.sent, "invalid message");
            for (auto & who : m.recipients) require(s.actors.contains(who), "unknown recipient");
            for (auto & who : m.readers) require(std::find(m.recipients.begin(), m.recipients.end(), who) != m.recipients.end(),
            "foreign reader");
        }
        std::set < Id > exids;
        for (const auto & e : s.external) {
            require(exids.insert(e.id).second && e.at >= 0, "duplicate/invalid external event");
            require(e.kind == "injury" || e.kind == "cash" || e.kind == "edge" || e.kind == "move_item" || e.kind == "shipment",
            "unregistered external effect");
            if (e.kind == "injury") {
                require(s.actors.contains(str(e.args, "actor")), "injury unknown actor");
                ratio(get < double >(e.args, "amount"), "injury");
            }
            if (e.kind == "edge") {
                require(s.edges.contains(str(e.args, "edge")), "unknown scripted edge");
                static_cast < void >(get < bool >(e.args, "open"));
            }
            if (e.kind == "shipment") {
                require(s.shops.contains(str(e.args, "shop")), "shipment unknown shop");
                static_cast < void >(c.item_type(str(e.args, "item_type")));
                ident(str(e.args, "item"));
                auto price = get < Money >(e.args, "price");
                require(price >= 0 && price <= max_money, "invalid shipment price");
            }
            if (e.kind == "move_item") {
                require(s.items.contains(str(e.args, "item")), "scripted move unknown item");
                auto p = decode < Placement >(at(e.args, "placement"));
                require((p.kind == "at" && s.places.contains(p.ref)) ||(p.kind == "inventory" && s.actors.contains(p.ref)) ||(p.kind == "in" && s.items.contains(p.ref)),
                "invalid scripted placement");
            }
            if (e.kind == "cash") {
                require(s.accounts.contains(str(e.args, "account")), "external unknown account");
                auto n = get < Money >(e.args, "amount");
                require(n >= - max_money && n <= max_money, "external amount out of range");
            }
        }
        if (restored) {
            std::set < Id > events;
            std::int64_t last = - 1;
            for (const auto & e : s.ledger) {
                require(events.insert(e.uid).second && e.sequence > last && e.time <= s.time, "event history invalid");
                last = e.sequence;
            }
            require(s.next_event_sequence > last, "event cursor mismatch");
        }
    }
}
