#pragma once
#include "npc/config.hpp"
#include "npc/local_view.hpp"
#include <functional>
namespace npc {
    class World {
        public : World(Config config, State initial);
        static World from_scenario(Config config, const Json & scenario);
        static World restore(Config config, const Json & snapshot);
        Receipt submit(const Command & command);
        bool can_decide(const Id & actor) const;
        TickReport advance();
        void run(Minute minutes);
        Json view(const Id & actor) const;
        ActorViewDelta view_delta(const Id& actor, const ActorViewCursor& cursor,
                                 std::size_t limit = local_page_limit,
                                 const std::vector<Id>& receipt_ids = {}) const;
        Json snapshot() const;
        const State & debug_state() const noexcept {
            return state_;
        }
        const Config & config() const noexcept {
            return config_;
        }
        void check_invariants() const;
        // Mental hypotheses are local records; no arbitrary physical deltas are accepted.
        void hypothesize(const Id & actor, const Evidence & hypothesis);
        private : World(Config config, State initial, bool restoring);
        Config config_;
        State state_;
        // Derived, actor-owned append-only history hashes. Rebuilt once on restore; not world state.
        std::map<Id, std::vector<std::string>> local_evidence_prefixes_;
        std::map<Id, std::vector<std::string>> local_inbox_prefixes_;
        void rebuild_local_evidence_index();
        std::string phase_ = "ready";
        std::size_t tick_event_start_ = 0;
        std::vector < Appraisal > pending_appraisals_;
        bool starting_ = false;
        bool at_place(const Id & actor, const Id & place) const;
        std::string item_place(const Id & item) const;
        std::string holder(const Id & item) const;
        double item_mass(const Id & item, std::set < Id > * visiting = nullptr) const;
        double carried_mass(const Id & actor) const;
        bool accessible(const Id & actor, const Id & item) const;
        bool permitted(const Id & actor, const Id & item) const;
        bool location_access(const Id & actor, const Id & place) const;
        bool awake(const Id & actor) const;
        bool open_interval(const Id & place, Minute start, Minute end) const;
        bool channel_available(const Id & from, const Id & to, const std::string & channel) const;
        bool record_access(const Id & actor, const RecordVersion & version) const;
        Money available(const Id & account, const Id & own_action = {}) const;
        bool lockable(const std::vector < Reservation > & locks, const Id & own = {}) const;
        bool try_start(const Command & command, const std::vector < Command > & batch);
        bool precommit(const Action & action) const;
        void commit(const Id & action_id);
        void interrupt(const Id & action_id, const std::string & reason, bool physical = false);
        void control(const Command & command);
        void release(const Id & action_id);
        void continuous();
        void physical_limits();
        void clocks();
        void information();
        void deliver_projections(std::size_t begin);
        void reactions();
        void thresholds();
        void finish_proposals_and_obligations();
        void reject(const Command & command, const std::string & debug);
        Event & emit(const Id & cause, const std::string & type, const std::vector < Id > & actors = {}, const std::vector < Id > & items = {},
        Json data = Object {}, bool owner_only = false, const Id & root = {}, Json secret = Object {});
        void remember(const Id & actor, Evidence evidence);
        void read_message(const Id & reader, const Id & message_id);
        void send_message(const Action & action, const Json & content, const std::vector < Id > & recipients, const std::vector < Id > & roots = {});
        void appraise(const Id & observer, const Id & target, const Id & root, const std::string & type);
        void move_item(const Id & item, const Placement & target, const Id & new_owner, const Action & action, const std::string & event_type);
        void transfer_money(const Id & from, const Id & to, Money amount, const Action & action);
        void complete_job(const Action & action);
        void transfer_action(const Action & action);
        void social_commit(const Action & action);
        void satisfy_obligation(Obligation & obligation, const Id & cause);
        void validate_terms(const Id & author, const std::set < Id > & participants, const Json & terms) const;
        bool known_proposal(const Id & actor, const Id & proposal, std::int64_t version) const;
    };
    // All inputs are caller-supplied local assumptions; this function has no World reference.
    Actor predict_physiology(const Config & config, Actor assumed, Minute minutes, const std::string & mode = "ordinary");
    double evaluate_belief(const Actor & actor, const Statement & statement, Minute now);
    std::string truth_value(const Actor & actor, const Statement & statement, Minute now);
}
