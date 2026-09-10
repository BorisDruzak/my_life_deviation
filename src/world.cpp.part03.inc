                auto & p = state_.proposals.at(pid);
                p.signatures.erase(c.actor);
                p.state = "withdrawn";
            }
            interrupt(active, "cancelled_by_participant");
        }
        if (! proposal.empty()) {
            auto & p = state_.proposals.at(proposal);
            p.signatures.erase(c.actor);
            p.state = "withdrawn";
            emit(command_key(c), "participation_withdrawn", {
                c.actor
            }, {}, Object {
                {
                    "proposal", proposal
                }
            }, true, p.root);
        }
        auto & r = state_.receipts.at(command_key(c));
        r.status = "completed";
    }
    void World::check_invariants() const {
        if (phase_ != "ready") throw InvariantError("invariants requested mid-step");
        validate_state(config_, state_, true);
    }
}
