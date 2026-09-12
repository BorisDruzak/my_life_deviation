#pragma once
#include "life/mind.hpp"
namespace life {
enum class SwitchReason:std::uint8_t {InsufficientGain,SameAction,IdleContinuation,BetterAlternative,CriticalRemedy};
struct SwitchAssessment{bool change=false;SwitchReason reason=SwitchReason::InsufficientGain;};
SwitchAssessment assess_interruption(Method current,double current_score,Method proposed,double proposed_score,const Outcomes& needs,double margin);
const char* switch_reason_name(SwitchReason reason);
} // namespace life
