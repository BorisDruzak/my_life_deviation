# NORM-MEMORY 0.1 implementation

Base: RECOVERY commit `d89ec4abf91f7d2f0febb290ef6849008003831a`.
The supplied plan names the earlier `36dc381` base; the intervening change is
MSVC build/test portability. The exact input package is preserved in
`provenance/incoming/norm_memory_0.1/package/`.

## Ownership and causal flow

`Mind::norm_memory` owns empirical beliefs and explicit personal principles.
`NormContextMemory` owns known group membership, goal links and questions.
Neither contains a global group roster, another actor's private model, or a
`world_root`. `NormSource` separates delivery identity from known source and
semantic revision. An unknown source stays unknown when forwarded.

The World publishes accessible projections into `CognitiveState::norm.inbox`.
Publication validates a detached candidate and never updates the actor's belief.
`InterpretNormObservation` and `IntegrateNormEvidence` run through the existing
cognition scheduler and operation budget. Accepted arguments use
`ReflectPersonalPrinciple` after interpretation. Gate closure, an exhausted
budget, or a changed own input prevents the unfinished update. Pending operands,
source revisions and continuations are serialized.

Before evaluation, paid `RecallNormContext` retrieves at most 32 candidate keys
from indexed own contexts. `CompareNormAlternatives` evaluates at most four,
further limited by the actor's current context slots. A focused question's key
is retained. Focus and metric are captured at the start of the paid operation.
Local contexts precede remote contexts; a remote group requires a known active
goal. A prepared view expires after 30 seconds or a change of own place,
knowledge, context revision or accessible audience. Workers receive only the resulting `NormDecisionView` and aligned
goal/audience information, not NormMemory or World access.

Physical clothing observations require recognition, current local visibility
and personally known membership. The sensor emits one person/practice/context/
day exposure; NormMemory separately enforces the dose across variants. Hidden
membership changes never update the known group. Familiarity alone does not
establish membership of every authored context. A personally known employment
contract change creates a new scoped group/goal without deleting old beliefs.
A hidden employment change in another actor is not an input.

## Statistical and moral content

Binary and three-category estimates, conditional detection/classification/
reaction, explicit severity and personal principles remain distinct. Severity
has its own knownness; a missing factor cannot become a zero expected sanction.
Known-source revisions replace contributions at their original time. Unknown
independence is pooled. Personal argument transforms replay in canonical order
from checkpoints; empirical decay does not erase personal commitments.

Cold records and source contributions are retained beyond the 128-entry derived
prediction cache. Context and prediction indexes are excluded from serialized
content and can be rebuilt. Cache reads cannot rejuvenate an observation.

Legacy personal arrays are imported as explicit `LegacyPrior` principles.
Public disapproval preserves its probability marginal; the unspecified approval
and indifference categories retain the symmetric prior's equal conditional odds.
The old aggregate TakeFood risk is represented by an explicit legacy expected
cost prior with the other factors equal to one, not invented institutional
observations. `bootstrap_norm_history` accepts only explicitly labelled
Historical projections at initial time and uses the same interpreter. Authored
paired scenarios supply their histories explicitly rather than relabelling
numeric priors as a biography.

## Decision and transport contracts

`DecisionLedger` rejects duplicate consequence keys. Forecast producers combine
estimates of the same outcome before inserting one term; probability, discount
and final squash are applied once. Joint-action consent remains separate from
audience approval. SELF contributes only its existing residual subjective costs;
forecasting does not train SELF. Foreign explanations do not fabricate personal
success. Actual approval or disapproval addressed to the actor uses the existing
OutcomeSignal and paid SELF operations.

Ask/Explain/Approve/Disapprove append to the existing interaction enum, preserving
old numeric values. `NormPayload::explanation` distinguishes reporting an approval
belief from personally expressing approval. Direct speech uses actual listener
consent. Phone content follows draft, send, delivery, physical read, social
interpretation and norm interpretation. The same typed payload and original
provenance survive each stage. Hearing another person report a personal
principle is not accepting an argument. Unsent explanations remain queued
across comparisons and are consumed by actual intent; repeated requests are
deduplicated. Refusal is not a whole group's opinion.

A ready question about clothing in the actor's known shop memory can recall
ClothingCondition/Tier from an already known, goal-linked future group even
when attention currently has another focus. It does not recall other practices
from that future group. The same paid Recall/Compare and 4-group, 32-key,
4-prediction limits apply. At equal option salience, the sign of the already paid applicable normative
consequence for an already goal-linked (G>0) exact ClothingTier can rank SKU
alternatives. This is a narrow attention tie rule, not a full utility sort;
with G=0 the baseline order remains and known sanctions still enter the paid
forecast if that option is considered. This uses
own known detection/audience, the goal significance, valued Approval,
applicability/exception and any known sanction. An unavailable or zero-valued
social outcome gives no positive priority. ClothingCondition cannot rank
prices; unknown, tied or goal-irrelevant beliefs do not reorder them.

The existing promise-related instrumental component of ReturnItem/RequestWork
is attributed to PersonalPrinciple rather than Enjoyment, without changing its
numeric raw value or discounting it twice. Real received Approve/Disapprove
uses revision 1 and the payload subject as the observed actor; the speaker
remains the source, and an absent subject stays unknown.

Clothing condition and tier are separate keys. Enabled deliberation considers
known affordable SKUs without a preselected desired-tier filter. The guarded
budget retains the reserve filter; the deliberative policy prices the marginal
reserve shortfall. Physical stock, actual payment and ownership remain executor
responsibilities. Norms create no automatic wages, fines or forced reactions.

Paid personal effects retain the exact NormKey, producer revision and origin in
up to four SocialEvaluation components and in ConsequenceTerm. The sensitivity/
trust cost of disclosure has a separate Procedure/Assumed owner backed by the
own SocialMemory revision; NoNormDecisionEffects retains this practical cost.
Paid own social appraisal excludes that cost from personal violation. Existing
moral clipping proportionally attributes the aggregate without changing its
Enabled numeric value. Multi-principle romance/claim, receiving Boundary and
Help/Promise effects retain separate provenance. Asocial Property and
PersonalRomance costs use the exact already-paid prediction.

The inherited public PartnerIntimacy replacement cost remains numerically 1 in
Legacy/Shadow and Enabled/Frozen, explicitly Procedure/Assumed source version 1
with no learned key. NoEffects excludes it. This preserves the inherited authored
rule, not a claim of learned evidence; replacing it with a learned Privacy rule
requires specification clarification. Physical participation and private-home
execution conditions remain independently enforced.

Both trace consumers use norm_trace_json, including complete key, ledger owner,
consequence identity and candidate identity. source_revision is the producer
version; it is not emitted as a known source root or an evidence event revision.
Traces do not change simulation state. Loaded active decisions validate component
bounds, finite costs and unique consequence keys before resumption.

## Modes and persistence

Legacy is the default. Shadow pays observation/thought costs while keeping the
legacy choice path. Enabled learns and uses norms. FrozenLearning preserves the
initial belief and records newly processed evidence in a shadow memory.
NoNormDecisionEffects processes evidence while excluding normative decision
terms and motives. Mode comparisons change the mechanism; the paired history
experiments isolate content while holding mode and physical start constant.

All modes use `LIFE-SAVE-0.14.0-norm01-r2`; 0.13 and the earlier draft r1 saves are rejected explicitly.
`--norm-memory` (alias `--norm-mode`), `--norm-state`, `--norm-trace`, and `--budget-policy` expose the
profile and diagnostics. Trace callbacks never enter the state hash. The profile
JSON is the documented compiled profile, not a runtime arbitrary JSON loader.

## Verification

The input matrix remains an acceptance specification, not a pass report.
`tools/run_norm_verification.py` and `tools/norm_behavior_scenarios.cpp` record
actual commands, return codes, source/executable/profile/state hashes and logs.
See `docs/verification/norm_memory_0.1/EXPERIMENT_PROTOCOL.md` for the predeclared
paired and free-world experiments. Final evidence and any unmet gates are
reported separately after execution; standalone arithmetic checks are not
simulation acceptance.
