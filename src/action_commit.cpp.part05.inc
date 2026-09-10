            }, {
                item
            }, Object {
                {
                    "shop", sh.id
                }, {
                    "item", item
                }, {
                    "holder", a.actor
                }, {
                    "place", sh.place
                }
            });
            Record rec;
            rec.id = a.id + "/receipt";
            rec.author = a.actor;
            RecordVersion v;
            v.time = state_.time + 1;
            v.acl = {
                a.actor
            };
            v.content = Object {
                {
                    "type", "receipt"
                }, {
                    "buyer", a.actor
                }, {
                    "seller", sh.id
                }, {
                    "item", item
                }, {
                    "price", a.price
                }
            };
            rec.versions.push_back(v);
            state_.records.emplace(rec.id, rec);
            emit(a.id, "record_created", {
                a.actor
            }, {}, Object {
                {
                    "record", rec.id
                }, {
                    "version", 1
                }
            }, true);
        } else if (a.type == "A08" || a.type == "A21") complete_job(a);
        else if (a.type == "A13") transfer_action(a);
        else if (a.type == "A25") {
            auto id = str(a.args, "resource");
            auto & acl = state_.permissions[id];
            if (acl.resource.empty()) {
                acl.resource = id;
                acl.authority = a.actor;
            }
            auto who = str(a.args, "grantee");
            if (get < bool >(a.args, "grant")) {
                acl.grantees.insert(who);
                acl.denied.erase(who);
            } else {
                acl.grantees.erase(who);
                acl.denied.insert(who);
            }
            ++ acl.version;
            emit(a.id, "access_changed", {
                a.actor, str(a.args, "grantee")
            }, {}, Object {
                {
                    "resource", id
                }, {
                    "granted", get < bool >(a.args, "grant")
                }, {
                    "version", acl.version
                }
            }, true);
        } else if (a.type == "A26") {
            auto & i = state_.items.at(str(a.args, "item"));
            i.open = get < bool >(a.args, "open");
            ++ i.version;
            emit(a.id, "container_state_changed", {
                a.actor
            }, {
                i.id
            }, Object {
                {
                    "item", i.id
                }, {
                    "open", i.open
                }
            });
        } else if (a.type == "A11" || a.type == "A12" || a.type == "A14" || a.type == "A17" || a.type == "A18" || a.type == "A19" || a.type == "A24" || a.type == "A27" || a.type == "A28" || a.type == "A29") social_commit(a);
        for (auto & key : a.commands) state_.receipts.at(key).status = "completed";
        emit(a.id, a.type == "A09" ? "sleep_ended" : a.type == "A15" ? "joint_ended" : "action_ended", a.participants, {},
        Object {
            {
                "action_type", a.type
            }
        }, a.type == "A31" || a.type == "A32");
        release(id);
    }
}
