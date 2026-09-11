#include "types_internal.hpp"
#include <algorithm>

namespace npc::policy::detail {
namespace {
void add_refs(std::vector<Id>& into,const std::vector<Id>& values) {
    into.insert(into.end(),values.begin(),values.end());
    std::sort(into.begin(),into.end()); into.erase(std::unique(into.begin(),into.end()),into.end());
}
std::string effect_key(const std::string& mode,const Id& obligation,const Id& actor,const Id& to,const Id& item={}) {
    return canonical(Array{mode,obligation,actor,to,item});
}
std::string session_effect_key(const Id& proposal,std::int64_t version,const Id& actor) {
    return canonical(Array{"session",proposal,version,actor});
}
bool closed(const std::string& status) {
    return status=="fulfilled" || status=="renegotiated" || status=="cancelled" || status=="interrupted";
}
void resolved(KnownCommitment& c,const std::string& status,Minute at) {
    c.status=status;
    c.resolved_at=c.resolved_at ? std::min(*c.resolved_at,at) : at;
    if (status=="fulfilled" && c.kind=="repay_money") c.remaining=0;
}
void reconcile(LocalData& data,KnownCommitment& c,const Id& actor) {
    if (c.status=="unknown") return;
    if (const auto* observed=data.obligation_observations.find(c.id)) {
        add_refs(c.source_refs,observed->source_refs);
        if (observed->creation) {
            const auto& o=*observed->creation;
            if (text(o,"debtor")!=c.debtor || text(o,"creditor")!=c.creditor || text(o,"kind")!=c.kind ||
                get_or<Minute>(o,"due",-1)!=c.due || (c.kind=="repay_money" && get_or<Money>(o,"amount",-1)!=c.amount)) {
                c.status="unknown"; c.resolved_at.reset(); return;
            }
            if (!closed(c.status)) { c.status="pending"; if(c.kind=="repay_money") c.remaining=c.amount; }
        }
        if (observed->fulfilled_at) resolved(c,"fulfilled",*observed->fulfilled_at);
    }
    const auto loan_mode=c.kind=="return_item" ? "loan_item" : "loan_money";
    const Id item=text(c.terms,"item");
    if (c.kind=="return_item" || c.kind=="repay_money") {
        if (const auto* start=data.receipt_effects.find(effect_key(loan_mode,c.proposal,c.creditor,c.debtor,item))) {
            add_refs(c.source_refs,start->source_refs);
            if (start->loan_started && !closed(c.status)) {
                c.status="pending"; if(c.kind=="repay_money") c.remaining=c.amount;
            }
        }
        const auto mode=c.kind=="return_item" ? "return" : "repay";
        if (const auto* done=data.receipt_effects.find(effect_key(mode,c.id,c.debtor,c.creditor,item))) {
            add_refs(c.source_refs,done->source_refs);
            if (!closed(c.status)) {
                if (c.kind=="return_item" && done->returned) resolved(c,"fulfilled",done->tick);
                if (c.kind=="repay_money" && done->repaid>0) {
                    if (done->repaid>c.amount) { c.status="unknown"; c.remaining.reset(); }
                    else {
                        // A completed repayment is itself evidence that a debt was incurred.
                        c.remaining=c.amount-done->repaid; c.status="pending";
                        if (*c.remaining==0) resolved(c,"fulfilled",done->tick);
                    }
                }
            }
        }
    }
    if (c.kind=="attend_session" || c.kind=="private_session") {
        if (const auto* done=data.receipt_effects.find(session_effect_key(c.proposal,c.version,actor))) {
            add_refs(c.source_refs,done->source_refs);
            if (!done->session_status.empty()) resolved(c,done->session_status,done->tick);
        }
    }
}
void store(LocalData& data,KnownCommitment c,const Id& actor) {
    const auto key=commitment_key(c.proposal,c.version);
    if (const auto* prior=data.commitments.find(key))
        data.deadlines.erase(ordinal_key(static_cast<std::uint64_t>(prior->due))+key);
    reconcile(data,c,actor);
    if (!closed(c.status) && c.status!="unknown") data.deadlines.set(ordinal_key(static_cast<std::uint64_t>(c.due))+key,key);
    const auto* existing_id=data.commitment_ids.find(c.id);
    if (!existing_id || data.commitments.find(*existing_id)->version<=c.version) data.commitment_ids.set(c.id,key);
    data.commitments.set(key,std::move(c));
}
void refresh_id(LocalData& data,const Id& id,const Id& actor) {
    if (const auto* key=data.commitment_ids.find(id)) {
        auto c=*data.commitments.find(*key); store(data,std::move(c),actor);
    }
}
void signed_terms(LocalData& data,const Evidence& e,const Id& actor) {
    const auto& p=e.statement.arguments;
    if (e.confidence!=1 || !e.statement.polarity || (e.kind!="observation" && e.kind!="claim") ||
        text(p,"state")!="accepted") return;
    const auto participants=get<std::set<Id>>(p,"participants"), signatures=get<std::set<Id>>(p,"signatures");
    if (!participants.contains(actor) || !participants.contains(e.source) || participants!=signatures || participants.size()<2) return;
    const auto proposal=str(p,"proposal"); const auto version=get<std::int64_t>(p,"version");
    if (proposal.empty() || version<1 || version>=max_minute) return;
    const auto& terms=at(p,"terms"); const auto kind=str(terms,"kind");
    KnownCommitment c;
    c.proposal=proposal; c.version=version; c.id=proposal+"/obligation"; c.root=e.roots.front();
    c.terms=terms; c.source_refs={e.id}; c.important=get_or<bool>(terms,"important",false);
    if (kind=="repair") {
        c.kind="repair"; c.status="pending"; c.debtor=str(terms,"debtor"); c.creditor=str(terms,"creditor");
        if (text(terms,"target").empty()) return;
    } else if (kind=="loan_item" || kind=="loan_money") {
        c.kind=kind=="loan_item" ? "return_item" : "repay_money";
        c.debtor=str(terms,"borrower"); c.creditor=str(terms,"lender"); c.status="agreed";
        if (kind=="loan_item" && text(terms,"item").empty()) return;
        if (kind=="loan_money") { c.amount=get<Money>(terms,"amount"); if(c.amount<=0 || c.amount>max_money) return; }
    } else if (kind=="joint" || kind=="private") {
        c.kind=kind=="joint" ? "attend_session" : "private_session"; c.id=proposal+"/session";
        c.debtor=actor; c.status="agreed"; c.due=get<Minute>(terms,"end");
        if (get<Minute>(terms,"start")<0 || get<Minute>(terms,"minutes")<1 ||
            c.due-get<Minute>(terms,"start")<get<Minute>(terms,"minutes")) return;
    } else if (kind=="reschedule") {
        const auto* old_key=data.commitment_ids.find(str(terms,"obligation"));
        if (!old_key) return; // Unknown original terms stay unknown; no global obligation lookup.
        auto old=*data.commitments.find(*old_key);
        if (participants!=std::set<Id>{old.debtor,old.creditor}) return;
        c=old; c.id=proposal+"/obligation"; c.proposal=proposal; c.version=version; c.root=e.roots.front();
        c.due=get<Minute>(terms,"due"); c.terms.as_object()["due"]=c.due;
        c.status=old.status=="pending" ? "pending" : "unknown"; c.resolved_at.reset();
        if (c.kind=="repay_money" && old.remaining) { c.amount=*old.remaining; c.remaining=c.amount; c.terms.as_object()["amount"]=c.amount; }
        add_refs(c.source_refs,{e.id});
        if (c.due<0 || c.due>=max_minute) return;
        resolved(old,"renegotiated",e.learned_at); add_refs(old.source_refs,{e.id}); store(data,std::move(old),actor);
    } else return;
    if (kind!="joint" && kind!="private" && kind!="reschedule") {
        c.due=get<Minute>(terms,"due");
        if (c.debtor==c.creditor || participants!=std::set<Id>{c.debtor,c.creditor}) return;
    }
    if (c.due<0 || c.due>=max_minute) return;
    if (const auto* prior=data.commitments.find(commitment_key(proposal,version))) {
        auto old=*prior; add_refs(old.source_refs,{e.id});
        // No last-writer-wins rule for incompatible signed versions of the same terms.
        if (canonical(old.terms)!=canonical(c.terms)) { old.status="unknown"; old.resolved_at.reset(); }
        store(data,std::move(old),actor);
    } else store(data,std::move(c),actor);
}
}
void ingest_commitment_evidence(LocalData& data,const Evidence& e,const Id& actor) {
    // An arbitrary human claim may contain malformed protocol-looking JSON. Retain the evidence,
    // but never turn it into a signed commitment or crash the controller because of its contents.
    try {
        if (e.statement.predicate=="proposal" || e.statement.predicate=="proposal_update") {
            signed_terms(data,e,actor); return;
        }
        if (e.kind!="observation" || e.confidence!=1 || !e.statement.polarity || e.statement.predicate!="event") return;
        const auto& event=e.statement.arguments; const auto type=text(event,"type");
        if (type!="obligation_created" && type!="obligation_fulfilled") return;
        const auto& payload=at(event,"data");
        const auto id=type=="obligation_created" ? str(payload,"id") : str(payload,"obligation");
        if (id.empty()) return;
        auto observation=data.obligation_observations.find(id) ? *data.obligation_observations.find(id) : ObservedObligation{};
        add_refs(observation.source_refs,{e.id});
        if (type=="obligation_created") observation.creation=payload;
        else observation.fulfilled_at=observation.fulfilled_at ? std::min(*observation.fulfilled_at,e.learned_at) : e.learned_at;
        data.obligation_observations.set(id,std::move(observation)); refresh_id(data,id,actor);
    } catch (const InputError&) { return; }
}
void ingest_receipt(LocalData& data,const LocalReceipt& r,const Id& actor) {
    const auto tick=r.observed_at;
    if (const auto* old=data.receipts.find(r.id)) {
        if (canonical(old->request)!=canonical(r.request) || old->actor!=r.actor) throw InputError("receipt id reused");
        if (old->status==r.status) return; // A repeated observation is not another repayment or another outcome.
        if (old->status=="completed" || old->status=="interrupted" || old->status=="rejected" ||
            (old->status=="started" && r.status=="queued") || r.observed_at<old->observed_at)
            throw InputError("receipt status regressed");
    }
    data.receipts.set(r.id,r);
    const auto c=decode<Command>(r.request);
    if (c.type=="A24" && r.status=="completed" && has(c.args,"message")) mark_message_read(data,str(c.args,"message"));
    if ((c.type=="A15" || c.type=="A32") && (r.status=="completed" || r.status=="interrupted")) {
        const auto proposal=text(c.args,"proposal"); const auto version=get_or<std::int64_t>(c.args,"version",0);
        if (proposal.empty() || version<1) return;
        const auto key=session_effect_key(proposal,version,actor);
        auto effect=data.receipt_effects.find(key) ? *data.receipt_effects.find(key) : ReceiptEffect{};
        // A later duplicate run cannot reopen a terminal participation. Replay order is irrelevant.
        if (effect.session_status.empty() || std::pair{tick,r.id}<std::pair{effect.tick,effect.terminal_receipt}) {
            effect.session_status=r.status=="completed" ? "fulfilled" : "interrupted";
            effect.tick=tick; effect.terminal_receipt=r.id;
        }
        add_refs(effect.source_refs,{r.id}); data.receipt_effects.set(key,std::move(effect));
        if (const auto* known=data.commitments.find(commitment_key(proposal,version))) store(data,*known,actor);
    }
    if (c.type!="A13" || r.status!="completed") return;
    const auto mode=text(c.args,"mode"), to=text(c.args,"to"), item=text(c.args,"item");
    const bool loan=mode=="loan_item" || mode=="loan_money";
    if (!loan && mode!="return" && mode!="repay") return;
    const auto ref=loan ? text(c.args,"proposal") : text(c.args,"obligation");
    if (ref.empty() || to.empty()) return;
    const auto key=effect_key(mode,ref,actor,to,item);
    auto effect=data.receipt_effects.find(key) ? *data.receipt_effects.find(key) : ReceiptEffect{};
    if (mode=="repay") {
        const auto amount=get_or<Money>(c.args,"amount",0);
        if (amount<=0 || amount>max_money || effect.repaid>max_money-amount) throw InputError("invalid/overflowing local repayment proof");
        effect.repaid+=amount;
    }
    if (mode=="return") effect.returned=true;
    if (loan) effect.loan_started=true;
    effect.tick=std::max(effect.tick,tick); add_refs(effect.source_refs,{r.id});
    data.receipt_effects.set(key,std::move(effect));
    refresh_id(data,loan ? ref+"/obligation" : ref,actor);
}
} // namespace npc::policy::detail
