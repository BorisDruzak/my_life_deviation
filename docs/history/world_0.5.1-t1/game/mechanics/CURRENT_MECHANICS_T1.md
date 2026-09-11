# Текущее состояние механик после T1

**Поставка:** `0.5.1-t1`. **Законы мира:** `WORLD-0.5.0`.

## Исполняемый контур

`Actor/State -> World::view -> controller::decide -> Command -> World::submit -> action_start -> minute_physics -> action_commit -> Event/Evidence/Receipt`.

Физика/World являются источником истины; debug ledger не является знанием NPC.

## Потребности

`N_next = clip(N_old + passive_delta + action_effect, 0, 100)`.

Текущие пороги: N01 food `critical=15, activation=45, target=80`; N02 sleep `10/35/85`; N03 social `10/40/75`; N04 leisure `5/35/75`; N05 optional `5/30/70`.

## Повторение, удовольствие, интерес

`repetition_next = repetition * exp(-1/tau) + exposure/minutes_per_unit`.

`pleasure = rate * exposure * (0.5 + interest) / (1 + novelty * repetition)`.

Interest learning уже исполняется при включённом профиле.

## Drive

`pressure_next = clip(pressure + intensity / pressure_growth_denominator_minutes - relief)`.

Но общего контура `drive -> Goal -> alternatives -> restraint -> autonomous deviant action/refusal` ещё нет.

## Совместное действие

A14 создаёт предложение; A12 — независимый ответ; A15 требует принятой конкретной версии и независимой текущей readiness всех участников.

`social_gain = (social_base + social_affection * mean_affection) * (1 - social_tension_factor * mean_tension) * participation / (60 * (1 + novelty * repetition))`.

Привязанность и напряжение обновляются только после фактического совместного времени.

## Личность

В `Actor` есть `traits`, `norm_prices`, `interests`, `skills`, `relations`, `drives`. Действующий controller частично использует `patience`, `caution`, `social_initiative`, `novelty`. Наличие остальных полей само по себе ещё не означает причинного влияния на общий выбор.

## T1

T1 добавляет `World::view_delta`, immutable `LocalView`, `ControllerMemory`, `KnownTrue/KnownFalse/Unknown`, известные обязательства и snapshot/restore policy memory. Общий GoalGenerator/Planner пока не управляет `world_sim`.

## Ещё не реализовано как общий контур

- GoalGenerator и lifecycle целей;
- MethodLibrary + bounded Planner;
- PlanMonitor;
- автономная социальная инициатива;
- общий accept/counter/decline по собственной альтернативе;
- DecisionTrace/потоковая отчётность;
- общий checkpoint World+scheduler+policy;
- причинная модель ценностей/характера/девиаций.
