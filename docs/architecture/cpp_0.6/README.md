# C++ 0.6 laboratory implementation

The active executable surface is the C++ 0.6 `mld` core.  Its build, target
graph and coverage boundary are described in [CPP_0.6.md](CPP_0.6.md).

Use these entry points according to their role:

- [BEHAVIOR-0.3 mechanics](../../rules/behavior_0.3/README.md) define the
  current behavioural material and its revision order.
- [Behaviour profile](../../../game/profiles/behavior_0.3/Параметры_поведения_v0.3.json)
  supplies the laboratory parameters.
- [Verification scripts](../../verification/behavior_0.3/) check the documented
  numerical contracts.
- [Laboratory reports](../../reports/cpp_0.6/) retain archive and supplied-run
  evidence. They are historical evidence, not proof of a fresh local run.

This is a limited laboratory profile. It does not claim to implement every
normative BEHAVIOR-0.3 document, a complete settlement simulation, or Unreal
Engine integration. The archive-origin planning document is retained as
[historical input](archive-plan-2026-09-11-cpp-0.6.md), not as a replacement
for the repository's approved migration plan.
