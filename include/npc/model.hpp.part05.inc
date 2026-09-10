        f("status", status);
        f("fulfilled_at", fulfilled_at);
        f("breached_at", breached_at);
        f("acknowledged", acknowledged);
    }
};
struct Action {
    Id id = {};
    std::string type = {};
    Id actor = {};
    std::vector<Id> participants = {};
    std::vector<Id> commands = {};
    Json args = Object{};
    Minute started = 0;
    Minute due = 0;
    Minute elapsed = 0;
    Id job = {};
    Id proposal = {};
    std::int64_t proposal_version = 0;
    Money price = 0;
    Json captured = Object{};
    Id intent = {};
    std::set<Id> entry_observers = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("type", type);
        f("actor", actor);
        f("participants", participants);
        f("commands", commands);
        f("args", args);
        f("started", started);
        f("due", due);
        f("elapsed", elapsed);
        f("job", job);
        f("proposal", proposal);
        f("proposal_version", proposal_version);
        f("price", price);
        f("captured", captured);
        f("intent", intent);
        f("entry_observers", entry_observers);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("type", type);
        f("actor", actor);
        f("participants", participants);
        f("commands", commands);
        f("args", args);
        f("started", started);
        f("due", due);
        f("elapsed", elapsed);
        f("job", job);
        f("proposal", proposal);
        f("proposal_version", proposal_version);
        f("price", price);
        f("captured", captured);
        f("intent", intent);
        f("entry_observers", entry_observers);
    }
};
struct RecordVersion {
    std::int64_t version = 1;
    Minute time = 0;
    Json content = Object{};
    std::set<Id> acl = {};
    bool public_read = false;
    bool deleted = false;
    template<class F> void visit(F&& f) {
        f("version", version);
        f("time", time);
        f("content", content);
        f("acl", acl);
        f("public_read", public_read);
        f("deleted", deleted);
    }
    template<class F> void visit(F&& f) const {
        f("version", version);
        f("time", time);
        f("content", content);
        f("acl", acl);
        f("public_read", public_read);
        f("deleted", deleted);
    }
};
struct Record {
    Id id = {};
    Id author = {};
    std::vector<RecordVersion> versions = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("author", author);
        f("versions", versions);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("author", author);
        f("versions", versions);
    }
};
struct Message {
    Id id = {};
    Id author = {};
    std::vector<Id> recipients = {};
    Json content = Object{};
    std::vector<Id> roots = {};
    std::string channel = "speech";
    Minute sent = 0;
    Minute deliver_at = 0;
    bool delivered = false;
    std::set<Id> readers = {};
    template<class F> void visit(F&& f) {
        f("id", id);
        f("author", author);
        f("recipients", recipients);
        f("content", content);
        f("roots", roots);
        f("channel", channel);
        f("sent", sent);
        f("deliver_at", deliver_at);
        f("delivered", delivered);
        f("readers", readers);
    }
    template<class F> void visit(F&& f) const {
        f("id", id);
        f("author", author);
        f("recipients", recipients);
        f("content", content);
        f("roots", roots);
        f("channel", channel);
        f("sent", sent);
        f("deliver_at", deliver_at);
        f("delivered", delivered);
        f("readers", readers);
    }
};
struct Event {
    Id uid = {};
    std::int64_t sequence = 0;
    Minute time = 0;
    std::string phase = {};
    std::string type = {};
    Id cause = {};
    Id root = {};
    std::vector<Id> participants = {};
    std::vector<Id> objects = {};
    Json data = Object{};
