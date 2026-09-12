#include "life/deliberation.hpp"
namespace life {
SwitchAssessment assess_interruption(Method current,double old,Method proposed,double score,const Outcomes& n,double margin){
 require_range(old,-1,1);require_range(score,-1,1);require_range(margin,0,1);
 if(proposed==current)return {false,SwitchReason::SameAction};
 if(proposed==Method::Idle)return {false,SwitchReason::IdleContinuation};
 const bool remedy=(n[0]>=.85&&(proposed==Method::Eat||proposed==Method::AcquireFood))||
  (n[1]>=.85&&proposed==Method::Drink)||(n[2]>=.85&&proposed==Method::Sleep)||
  (n[3]>=.85&&proposed==Method::Rest)||(n[8]>=.85&&proposed==Method::Shelter);
 if(remedy)return {true,SwitchReason::CriticalRemedy};
 if(current==Method::Idle||score>old+margin)return {true,SwitchReason::BetterAlternative};
 return {};
}
const char* switch_reason_name(SwitchReason r){switch(r){
 case SwitchReason::InsufficientGain:return "continue_insufficient_gain";
 case SwitchReason::SameAction:return "continue_same_activity";
 case SwitchReason::IdleContinuation:return "continue_no_executable_alternative";
 case SwitchReason::BetterAlternative:return "switch_better_alternative";
 case SwitchReason::CriticalRemedy:return "switch_specific_critical_remedy";
 }return "invalid_switch";}
}
