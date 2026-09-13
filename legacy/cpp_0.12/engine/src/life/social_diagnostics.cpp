#include "life/world.hpp"
#include <iomanip>
#include <sstream>

namespace life {
namespace {
const char* truth_name(Truth t){static constexpr const char* n[]={"unknown","confirmed","refuted","conflicting"};return n[unsigned(t)];}
const char* fact_name(FactOrigin o){static constexpr const char* n[]={"initial_belief","observed","reported","agreement"};return n[unsigned(o)];}
const char* norm_name(std::size_t i){static constexpr const char* n[]={"respect_boundary","privacy","property","modesty","personal_romance","honesty","promise"};return n[i];}
}
std::string World::social_report_json()const{
    // Diagnostic export only: it deliberately separates omniscient world records
    // from each person's own beliefs. Neither section is fed into the planner.
    std::ostringstream out;out<<std::setprecision(17)<<"{\"version\":\"0.10.0\",\"ms\":"<<state_.now<<",\"world_objects\":[";
    bool comma=false;for(const auto& o:state_.social.objects){if(comma)out<<',';comma=true;out<<"{\"id\":"<<o.id<<",\"owner\":"<<o.owner<<",\"holder\":"<<o.holder<<",\"kind\":"<<unsigned(o.kind)<<",\"uses\":"<<o.uses<<",\"active_loan\":"<<o.active_loan<<'}';}
    out<<"],\"loans\":[";comma=false;for(const auto& l:state_.social.loans){if(comma)out<<',';comma=true;out<<"{\"id\":"<<l.id<<",\"object\":"<<l.object<<",\"lender\":"<<l.lender<<",\"borrower\":"<<l.borrower<<",\"due\":"<<l.due<<",\"returned\":"<<(l.returned?"true":"false")<<'}';}
    out<<"],\"events\":[";comma=false;for(const auto& e:state_.social.events){if(comma)out<<',';comma=true;
        out<<"{\"id\":"<<e.id<<",\"parent\":"<<e.parent<<",\"a\":"<<e.initiator<<",\"b\":"<<e.receiver<<",\"object\":"<<e.object<<",\"interaction\":\""<<interaction_name(e.kind)<<"\",\"phase\":"<<unsigned(e.phase)<<",\"reason\":\""<<social_reason_name(e.reason)<<"\",\"proposed_ms\":"<<e.proposed_at<<",\"answered_ms\":"<<e.answered_at<<",\"due_ms\":"<<e.ends<<",\"public_a\":"<<(e.public_a?"true":"false")<<",\"public_b\":"<<(e.public_b?"true":"false")<<",\"reply_score\":"<<e.response.score<<",\"reply_moral\":"<<e.response.moral<<",\"pleasure_a\":"<<e.pleasure_a<<",\"pleasure_b\":"<<e.pleasure_b<<",\"approval_a\":"<<e.approval_a<<'}';}
    out<<"],\"actors\":[";comma=false;for(const auto& a:state_.actors){if(comma)out<<',';comma=true;const auto& m=a.mind.social;
        out<<"{\"id\":"<<a.id<<",\"gender\":"<<unsigned(m.gender)<<",\"social_saturation\":"<<a.social<<",\"esteem\":"<<a.esteem<<",\"shame\":"<<a.affect.total[std::size_t(Emotion::Shame)]<<",\"guilt\":"<<a.affect.total[std::size_t(Emotion::Guilt)]<<",\"status_importance\":"<<m.status_importance<<",\"goal_object\":"<<m.goal_object<<",\"goal_used\":"<<(m.goal_used?"true":"false")<<",\"learned_outcomes\":"<<m.learned_outcomes<<",\"learned_reports\":"<<m.learned_reports<<",\"refusals\":"<<m.refusals_received<<",\"pressure_received\":"<<m.pressure_received<<",\"knowledge\":[";
        for(std::size_t k=0;k<interaction_count;++k){if(k)out<<',';const auto& b=m.methods[k];out<<"{\"interaction\":\""<<interaction_name(Interaction(k))<<"\",\"mastery\":"<<b.mastery<<",\"expected_pleasure\":"<<b.expected_pleasure<<",\"expected_acceptance\":"<<b.expected_acceptance<<",\"confidence\":"<<b.confidence<<",\"source\":"<<b.source<<",\"debug_primary_response\":"<<a.social_traits.pleasure[k]<<'}';}
        out<<"],\"norms\":{";for(std::size_t k=0;k<social_norm_count;++k){if(k)out<<',';out<<'"'<<norm_name(k)<<"\":"<<m.norms[k];}
        out<<"},\"known_people\":[";bool c=false;for(const auto& p:m.people){if(c)out<<',';c=true;out<<"{\"id\":"<<p.id<<",\"gender\":"<<unsigned(p.gender)<<",\"source\":"<<p.source<<'}';}
        out<<"],\"items\":[";c=false;for(const auto& i:m.items){if(c)out<<',';c=true;out<<"{\"id\":"<<i.id<<",\"owner\":"<<i.owner<<",\"holder\":"<<i.holder<<",\"owner_status\":\""<<truth_name(i.owner_status)<<"\",\"holder_status\":\""<<truth_name(i.holder_status)<<"\",\"origin\":\""<<fact_name(i.origin)<<"\",\"source\":"<<i.source<<",\"expected_use\":"<<i.expected_use<<",\"prestige\":"<<i.prestige<<",\"uses\":"<<i.uses<<",\"claims\":[";
            bool d=false;for(const auto& q:i.claims){if(d)out<<',';d=true;out<<"{\"source\":"<<q.source<<",\"speaker\":"<<q.speaker<<",\"owner\":"<<q.owner<<",\"holder\":"<<q.holder<<",\"origin\":\""<<fact_name(q.origin)<<"\",\"at\":"<<q.at<<'}';}out<<"]}";}
        out<<"],\"experiences\":[";c=false;for(const auto& e:m.experiences){if(c)out<<',';c=true;out<<"{\"interaction\":\""<<interaction_name(e.kind)<<"\",\"person\":"<<e.person<<",\"object\":"<<e.object<<",\"public\":"<<(e.public_context?"true":"false")<<",\"pleasure_mean\":"<<e.pleasure.mean<<",\"pleasure_weight\":"<<e.pleasure.count<<",\"pleasure_confidence\":"<<e.pleasure.confidence()<<",\"approval_mean\":"<<e.approval.mean<<",\"approval_weight\":"<<e.approval.count<<",\"accepted_prior_inclusive\":"<<e.accepted<<",\"refused_prior_inclusive\":"<<e.refused<<",\"attempts\":"<<e.attempts<<",\"issued_boundaries\":"<<e.issued_boundaries<<",\"incoming\":"<<e.incoming<<",\"last_event\":"<<e.last_event<<'}';}
        out<<"],\"pending_messages\":"<<m.inbox.size()<<'}';
    }
    out<<"]}";return out.str();
}
}
