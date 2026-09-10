        f("place", place);
        f("account", account);
        f("slots", slots);
        f("stock", stock);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("place", place);
        f("account", account);
        f("slots", slots);
        f("stock", stock);
    }
};
struct Permission {
    Id resource = {};
    Id authority = {};
    std::set<Id> grantees = {};
    std::set<Id> denied = {};
    std::int64_t version = 1;
    template<class F> void visit(F&& f) {
        f("resource", resource);
        f("authority", authority);
        f("grantees", grantees);
        f("denied", denied);
        f("version", version);
    }
    template<class F> void visit(F&& f) const {
        f("resource", resource);
        f("authority", authority);
        f("grantees", grantees);
        f("denied", denied);
        f("version", version);
    }
};
struct Contract {
    Id id = {};
    Id worker = {};
    Id employer_account = {};
    Id workplace = {};
    Money rate_per_hour = 60;
    Money accrued_numerator = 0;
    Minute next_payout = 1440;
    Minute payout_period = 1440;
    std::vector<std::vector<Minute>> windows = {{480,1020}};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("worker", worker);
        f("employer_account", employer_account);
        f("workplace", workplace);
        f("rate_per_hour", rate_per_hour);
        f("accrued_numerator", accrued_numerator);
        f("next_payout", next_payout);
        f("payout_period", payout_period);
        f("windows", windows);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("worker", worker);
        f("employer_account", employer_account);
        f("workplace", workplace);
        f("rate_per_hour", rate_per_hour);
        f("accrued_numerator", accrued_numerator);
        f("next_payout", next_payout);
        f("payout_period", payout_period);
        f("windows", windows);
    }
};
struct Command {
    Id actor = {};
    std::int64_t sequence = 0;
    std::string type = "A01";
    Json args = Object{};
    std::map<Id,std::int64_t> expected_versions = {};
    Id intent = {};
    template<class F> void visit(F&& f) {
        f("actor", actor);
        f("sequence", sequence);
        f("type", type);
        f("args", args);
        f("expected_versions", expected_versions);
        f("intent", intent);
    }
    template<class F> void visit(F&& f) const {
        f("actor", actor);
        f("sequence", sequence);
        f("type", type);
        f("args", args);
        f("expected_versions", expected_versions);
        f("intent", intent);
    }
};
struct Receipt {
    std::string fingerprint = {};
    Json request = Object{};
    Id actor = {};
    Id action = {};
    std::string status = "queued";
    std::string public_reason = {};
    std::string debug_reason = {};
    Minute received = 0;
    template<class F> void visit(F&& f) {
        f("fingerprint", fingerprint);
        f("request", request);
        f("actor", actor);
        f("action", action);
        f("status", status);
        f("public_reason", public_reason);
        f("debug_reason", debug_reason);
        f("received", received);
    }
    template<class F> void visit(F&& f) const {
        f("fingerprint", fingerprint);
        f("request", request);
        f("actor", actor);
        f("action", action);
        f("status", status);
        f("public_reason", public_reason);
        f("debug_reason", debug_reason);
        f("received", received);
    }
};
struct Reservation {
    Id id = {};
    Id resource = {};
    Id action = {};
    Money amount = 1;
    Money capacity = 1;
    Minute until = 0;
    template<class F> void visit(F&& f) {
        f("id", id);
        f("resource", resource);
        f("action", action);
        f("amount", amount);
        f("capacity", capacity);
        f("until", until);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
