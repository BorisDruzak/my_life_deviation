# Реализация автономности NPC — план для Codex

> Для agentic workers: выполнять отдельными проверяемыми изменениями; при доступных skills использовать superpowers:subagent-driven-development или superpowers:executing-plans. Текущий статус: T0 и T1 выполнены и проверены; T2–T9 ещё не реализованы. См. `docs/implementation/STATUS.md`.

**Goal:** реализовать общий локальный выбор целей, ограниченные планы, независимую инициативу conversation и объясняющие отчёты без чтения скрытого состояния.

**Architecture:** неизменные границы World/LocalView, отдельные policy-модули, общий чистый прогноз физических эффектов, write-only telemetry. Сначала диагностируемые ошибки и контракты, затем генератор/планировщик, затем социальная инициатива и сквозная проверка.

**Tech Stack:** C++20, CMake>=3.20, существующий JSON и native test harness; Python3 для CLI-тестов. Новых внешних runtime-зависимостей не требуется.

**Spec:** `docs/design/autonomy_2026-09-10/02_GOALS_AND_CONTRACTS.md` … `06_ACCEPTANCE.md`; реестр правил `08_RULE_DECISIONS.md`.

## Общие ограничения

Работать из пользовательского WORLD-0.5, не смешивать произвольный более новый main без отдельного diff. Цифровое delivery≠read; подпись≠готовность; прогноз≠результат; owner≠holder≠permission. World не получает мотивов для принуждения участника. Все расходы/эффекты возникают через существующий исполнитель или явно версионированное новое физическое действие.

Начальный профиль: autonomous pairs, conversation; optional shared_reading/ball_game отдельным изменением после регистрации физических контрактов. Не изменять коэффициенты world, чтобы скрыть ошибки политики. POLICY-0.1 tuning хранить отдельно.

Сохранить исходный regression suite. Новые native tests подключать в CMake; `--case NAME` уже поддерживается. Нулевое число запущенных tests не считать PASS. На небольшом проекте CTest после каждого законченного изменения допустим: проверенный исходный suite занял секунды, но это не гарантия времени будущего набора.

## Карта файлов

| Создать | Ответственность |
|---|---|
| include/npc/policy/types.hpp, src/policy/types.cpp | Goal, Plan, immutable LocalView, ControllerMemory, сериализация локальных типов |
| include/npc/policy/knowledge_query.hpp, src/policy/knowledge_query.cpp | Временная применимость, true/false/unknown, источники и conflict |
| include/npc/policy/goals.hpp, src/policy/goals.cpp | Нужды, обязательства, проекты, identity/episodes и shortlist |
| include/npc/policy/methods.hpp, src/policy/methods.cpp | Типизированные условия/ресурсы/методы, ленивые bindings |
| include/npc/policy/predictor.hpp, src/policy/predictor.cpp | Чистый локальный минутный прогноз собственного состояния |
| include/npc/policy/planner.hpp, src/policy/planner.cpp | SearchBudget, beam, доминирование, utility, partial result |
| include/npc/policy/social_policy.hpp, src/policy/social_policy.cpp | Независимые предложения/ответы, сессии, cooldown, собственный календарь |
| include/npc/policy/monitor.hpp, src/policy/monitor.cpp | Минутные safety/receipt/session checks, валидность suffix |
| include/npc/reporting.hpp, src/reporting.cpp | NeedDelta, агрегаты, DecisionTrace, privacy/scopes |
| data/policy_defaults.json | Лимиты/коэффициенты новой политики, отдельный version/hash |
| data/activities.json | Закрытые activity definitions с версией физического эффекта |
| tests/policy_goals_tests.cpp, policy_planner_tests.cpp, policy_social_tests.cpp, reporting_tests.cpp | Изолированная и сквозная приёмка по IDs из acceptance_cases.json |
| tests/autonomy_cli_test.py | Полный цикл, snapshot, отчёты, реальные регрессионные сценарии |

Изменять `src/controller.cpp` и `include/npc/controller.hpp` как фасад совместимости, `app/main.cpp` для scheduling/reporting/checkpoint, `src/knowledge.cpp` для безопасной локальной проекции/курсора, `src/persistence.cpp` для SimulationCheckpoint. `src/dynamics.cpp`/`physical.cpp` изменять только для общего чистого расчёта и диагностического sink; `social.cpp`, `action_start.cpp`, `action_commit.cpp`, `config.cpp` — для версионированного activity-контракта. Не переносить всё в ещё один 2000-строчный controller.cpp.

## T0. Зафиксировать и устранить регрессии текущей политики

**Files:** `src/controller.cpp:314–419,471–565`, `tests/controller_tests.cpp`, `tests/autonomy_cli_test.py`, `CMakeLists.txt`; фикстуры взять из `reports/audit_2026-09-10/fixtures/`.

**Consumes:** текущий `decide(Config,Json,Json)`, собственные `view.receipts` и готовая еда.
**Produces:** неблокирующее ожидание и terminal handling для существующего household; физика без изменений.

- [x] Добавить regression test ожидания: на готовой фикстуре a должен начать A07 прежде N01<=15. Сначала получить FAIL на исходном коде, сохранив событийный лог.
- [x] Добавить regression test interrupted: после t=3 не должно быть новой A15 для `a@1/proposal`; сначала получить FAIL с нынешними32rejects.
- [x] Сохранить отрицательный контроль completed: одно завершение, ноль повторов. Не исправлять то, что уже работает.
- [x] Временно вынести safety check выше обработки inbox/proposals; `await_response`/`await_handoff` сделать причиной фонового ожидания, не ранним обязательным A01.
- [x] Для собственной терминальной A15 проверять proposal **и version**, status completed/interrupted; не применять правило «никогда не повторять» ко всем rejected A15 — неполная текущая готовность может быть временной.
- [x] Прогнать оба red→green, отрицательный контроль, CTest. Commit отдельно `fix: avoid blocking needs and replaying interrupted joint sessions`.

Полный CLI-assertion для interrupted (после запуска той же фикстуры):

```python
import json
from pathlib import Path
rows = [json.loads(x) for x in Path(out, 'decisions.jsonl').read_text().splitlines()]
for decision in rows:
    command = decision.get('command')
    if decision['time'] > 3 and command and command['type'] == 'A15':
        assert command['args']['proposal'] != 'a@1/proposal', decision
```

Это проверяет не только число rejected events, но и само повторное намерение. В реализации новой trace schema заменить имя time на tick через явную версию теста, не угадывать поле.

## T1. Зафиксировать собственные типы, знания и память

**Files:** policy/types.*, knowledge_query.*, `src/knowledge.cpp`, `tests/policy_goals_tests.cpp`.
**Consumes:** семантика собственных данных World.view(actor), реализованная прямой bounded-проекцией World::view_delta; правила и собственные receipts. Старый полный view не копируется перед обрезанием.
**Produces:** immutable LocalView, BeliefResult, ControllerMemory; planner не может принять World/State.

- [x] Добавить контрфактический тест P03: изменить чужие скрытые needs/relations, оставить LocalView одинаковым; решения должны совпасть.
- [x] Реализовать ограниченную ActorViewDelta-проекцию по курсору, чтобы не копировать всю историю до начала bounded search; старый полный view сохранить для export. Реализовать KnownTrue/KnownFalse/Unknown, причины и источники временной гипотезы. На конфликте полярностей не выбирать последнюю удобную запись без правила.
- [x] Отдельно извлечь KnownCommitment из собственных подписанных условий и receipts. Не выдавать глобальный obligations.status в view, если изменение недоступно NPC.
- [x] Сериализовать поля GC-11 с version/config hash; валидация unknown status, negative tick и дублирующихся goal keys возвращает InputError.
- [x] Проверить G09/G12, P03/P13; повторить CTest; commit `feat: add local policy state and evidence queries`.

**Фактическая граница T1:** все 36 новых C++-кейсов и прежние 122 прошли. G09/G12/P03/P13 проверены на компонентах local memory/query; следующие scores/trace/commands новой политики и ResolveUncertainty ещё не существуют. Полная приёмка этих сценариев остаётся в T3/T5/T8. См. `reports/implementation/T1/acceptance_mapping.json` и отчёт T1.

## T2. Потоковые потребности и трасса фактического выбора

**Files:** reporting.*, `src/dynamics.cpp`, `src/physical.cpp`, `app/main.cpp`, `tests/reporting_tests.cpp`.
**Consumes:** те же уже рассчитанные до/после значения, реальные Decision/Command/Receipt.
**Produces:** RP-03 NeedDelta, exact window aggregates, versioned DecisionTrace/OutcomeLink, manifest.

- [ ] Red-tests R01…R05: clamp identity, половинный интервал окна, цензурирование, N05 disabled, terminal sample duplicate.
- [ ] Выделить сырые drift/effect значения, не менять порядок суммирования мира; вычислять clamp_adjustment после реально применённой формулы.
- [ ] Реализовать summary/full sink, no-command trace и приватные scopes; score=null для неоценённых вариантов. Старый describe остаётся поддержан для исходного CLI режима.
- [ ] Подключить tests R06…R14, особенно разные знаменатели и report on/off hash. Failure записи возвращает явный status/exit code.
- [ ] CTest, commit `feat: record exact need exposure and decision provenance`.

Простой независимый эталон для R02 (не замена C++-агрегатора):

```python
values = [10.0, 20.0, 30.0]  # [0,3), final=40 excluded
assert sum(values) / len(values) == 20.0
assert sum(n < 25.0 for n in values) == 2
assert sum(n <= 10.0 for n in values) == 1
assert sum(max(0.0, 75.0 - n) for n in values) == 165.0
```

## T3. Общий генератор целей и полезность

**Files:** goals.*, types.*, planner utility section, policy_defaults.json, `tests/policy_goals_tests.cpp`.
**Consumes:** LocalView/ControllerMemory, own needs/thresholds and known obligations.
**Produces:** GoalUpdate с identity, latch, source, urgency, review time; единая версия utility.

- [ ] Реализовать G01/G02: одна goal episode до достижения target, новая только после повторной активации.
- [ ] Добавить GC family triggers, дедупликацию acquire по родителям, конечный бюджет, сроки возврата/оплаты и ResolveUncertainty только для полезной цели.
- [ ] Взять формулы строго из §2.5–2.7. Все альтернативы на одном H; C_switch применяется **один** раз. Поддержать явное включение N05, без парного private-метода.
- [ ] Добавить G06…G11 и аналитический urgency-test: при C10,A40,T75 значение n40→0, n25→0.5, n10→1, n0→3; invalid C0 отклоняется.
- [ ] CTest, commit `feat: generate persistent goals with comparable utility`.

Содержимое аналитического теста после введения объявленного utility API:

```cpp
// Контракт новой функции в goals.hpp:
// double need_urgency(double n, double critical, double activation);
TEST(PG_need_urgency) {
    NEAR(npc::policy::need_urgency(40, 10, 40), 0, 1e-12);
    NEAR(npc::policy::need_urgency(25, 10, 40), .5, 1e-12);
    NEAR(npc::policy::need_urgency(10, 10, 40), 1, 1e-12);
    NEAR(npc::policy::need_urgency(0, 10, 40), 3, 1e-12);
    THROWS(npc::policy::need_urgency(20, 0, 40));
}
```

## T4. Общие методы и чистый профильный прогноз

**Files:** methods.*, predictor.*, `src/dynamics.cpp`, `src/physical.cpp`, `tests/policy_planner_tests.cpp`.
**Consumes:** собственный ForecastState, известные methods, immutable world configuration.
**Produces:** MethodInstance, precondition result, conditional Prediction с mode/version и source_refs.

- [ ] Добавить P01/P02/P04/P05 до реализации: купить→съесть, приготовить→съесть, чистота forecast, равенство мира/прогноза для собственных известных действий.
- [ ] Общий расчёт effects вынести так, чтобы WORLD исполнял реальные ресурсы, predictor — только своё виртуальное состояние. Не вызывать World.submit внутри прогноза.
- [ ] Явно разделить start/invariant/end, duration и interruptibility. Borrow требует будущего return; готовая еда имеет units и однократное списание.
- [ ] Использовать текущие физические формулы baseline/extended, собственные interest/repetition; ошибки неподдержанного effect_model явные, не пустой «успешный» прогноз.
- [ ] P01/P02/P04/P05/P10, CTest, commit `feat: add local method models and profile-aware forecasts`.

## T5. Ограниченный поиск и монитор

**Files:** planner.*, monitor.*, `src/controller.cpp`, `app/main.cpp`, `tests/policy_planner_tests.cpp`.
**Consumes:** goal shortlist, lazy methods, LocalPredictor, ControllerMemory.
**Produces:** PlanningResult и safe prefix, trace budgets; Monitor выполняется даже при active_action.

- [ ] Первые red-tests: один лимит на каждом этапе; отсутствие скрытого полного Cartesian/BFS до счётчика; cycle stop; event wake при busy.
- [ ] Реализовать лимиты §5.4, bounded beam, одинаковый H, memo/dominance по ресурсам/долгам/времени и stable tie-break. Все counters принадлежат одному решению.
- [ ] Реализовать статусы §5.6 без смешения unknown/false/budget. Внешний ответ обрывает исполняемый префикс; на чужой predicted accept не выполнять реальное действие.
- [ ] Monitor каждую минуту, fullsearch по событию/фазе5мин, максимум один fullsearch в tick; текущие A22/A02 ограничения сохранить.
- [ ] G03/G04/G05/G10, P06…P14/P16; CTest; commit `feat: execute bounded local plans with safe replanning`.

## T6. Физический контракт conversation и закрытие темы

**Files:** `data/activities.json`, `src/config.cpp`, `social.cpp`, `action_start.cpp`, `action_commit.cpp`, `physical.cpp`, `tests/policy_social_tests.cpp`.
**Consumes:** действующие proposals/version/signatures, известные темы и новое activity_id.
**Produces:** валидируемый conversation без строкового обхода repetition, существующая independent A15 readiness.

- [ ] Red-test J08 на незарегистрированной теме и J09 на смене разрешённых тем одного семейства.
- [ ] Ввести новую версию activity schema. Импорт старой версии выполнять явно: известная conversation преобразуется, неизвестная строка требует ручного решения/ошибки, а не нового физического семейства.
- [ ] Разделить semantic topic, interest topic и repetition key; attention общий <=1. Не менять rates старого разговора в этом изменении.
- [ ] Сохранить J12: одного accept и одной A15 недостаточно для старта; A22 немедленно прекращает будущие эффекты.
- [ ] J07…J09/J12/J16/J17, CTest, commit `feat: validate joint activity semantics and repetition families`.

## T7. Автономные предложения, ответы и собственные сессии

**Files:** social_policy.*, monitor.*, methods.*, policy_defaults.json, `tests/policy_social_tests.cpp`, `tests/autonomy_cli_test.py`.
**Consumes:** own social goal, known partner/contact/place, own utility, read answers.
**Produces:** A14/A12/A24/A15/A22 только от самого actor, SessionRuntime и cooldown.

- [ ] Создать благоприятную пару без scripted social commands; затем независимый отказ и counter. Модифицировать скрытые needs адресата: инициатор до ответа не должен их угадать.
- [ ] Реализовать lazy partner/activity selection; только conversation и пары включены по умолчанию. Поведение иных групп проверяется внешними предложениями, не выдается за автономную генерацию групп.
- [ ] Реализовать независимую оценку Delta, counter<=2, подписи версии, bounded readiness wait, terminal receipt и неблокирующие waiting goals.
- [ ] Дедуплицировать историю ответов по read roots; prior не бросает монетку за адресата. Обычный decline/timeout не штрафуют trust.
- [ ] Разрешить пересекающиеся взаимные приглашения правилом JA-12 без лишнего договора и без тайного доступа. Необходимость этого теста нельзя убрать «низкой вероятностью совпадения».
- [ ] J01…J07/J12…J19, G03, R06/R07/R10; CTest; commit `feat: let independent NPCs initiate and negotiate joint sessions`.

## T8. Полный checkpoint и воспроизводимые отчёты

**Files:** types.*, `src/persistence.cpp`, `app/main.cpp`, reporting.*, `tests/autonomy_cli_test.py`.
**Consumes:** World snapshot и вся собственная память policy/reporter.
**Produces:** SimulationCheckpoint с version/hash/cursor, summary отчёты и воронка.

- [ ] Red-tests resume во время движения, ожидания и готовности; отдельно report on/off/full, actor privacy, duplicate roots.
- [ ] Сохранить policy memory/sequence/cooldowns/fair cursor/next review и report accumulators. Старый snapshot явно импортируется с policy_state_missing, не объявляется тождественным продолжением.
- [ ] Построить полный end-to-end социальный funnel, связать need episode→decision→command→outcome; stale legacy CSV сохранять sampled.
- [ ] R01…R14, G12, P03/P11/P12; CTest; commit `feat: checkpoint autonomous policy and export causal reports`.

## T9. Расширить реальные занятия и провести масштабные опыты

**Files:** activities.json, methods.*, physical.cpp/action_start.cpp/action_commit.cpp, `tests/policy_social_tests.cpp`; новые examples `autonomy_pairs.json`, `autonomy_constrained.json`, `autonomy_scale.json`.
**Consumes:** законченный conversation-контур; собственные законные ресурсы и непрерывная валидация.
**Produces:** shared_reading/ball_game как физические занятия, измеренная нагрузка10/100/1000.

- [ ] J10/J11: нет права/нагрузка запрещена — нет старта; есть все условия — book/ball lock один, эффекты каждому без двойного pleasure.
- [ ] Проверить прекращение при утрате ресурса/доступа, group normalization и общую repetition family с одиночным действием.
- [ ] Провести благоприятную/ограниченную недельную пару и household; сравнить точную экспозицию, причины незавершённых episodes и rejected commands, не только end N03.
- [ ] Измерить P15 на закреплённых hardware/config; опубликовать raw counters и границы. Не утверждать realtime до измерения целевого железа.
- [ ] Обновить нормативные WORLD/API/ACTIONS/PARAMETERS/ACCEPTANCE/LIMITATIONS только по фактически реализованным результатам. Commit `feat: add resource-backed group leisure and autonomy benchmarks`.

## Общие команды проверки

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/npc_tests --list
./build/npc_tests --case C01_hungry_uses_owned_food
ctest --test-dir build --output-on-failure
```

Для каждого нового теста команда `--case` использует реально зарегистрированное имя, проверяемое через `--list`. В T0 запустить зафиксированные диагностические fixtures через сохранённый runner и увидеть ожидаемый переход старых дефектов; старый audit runner по умолчанию проверяет **старое поведение**, его успешность не является критерием исправленного релиза.

## Архив промпта первой задачи (T0 выполнен)

> Прочитай docs/design/autonomy_2026-09-10/00_README.md, 01_AUDIT.md и раздел T0 плана. Работа только над T0: воспроизведи ожидание вместо еды и повторы прерванной встречи, добавь failing tests, исправь household-политику без изменений физики, сохрани отрицательный контроль обычного завершения. Не внедряй всю POLICY-0.1 в один commit. Покажи команды, red/green результаты, изменённые файлы и оставшиеся границы. Затем остановись на ревью T0.


## Следующая задача в этом чате: T2

> Прочитай `docs/implementation/STATUS.md`, `reports/implementation/T1/VERIFICATION.md`,
> `docs/implementation/T1_LOCAL_POLICY_STATE.md` и раздел T2. Сохрани 158 C++-кейсов,
> прежний CLI и пять сценариев autonomy_t0. Начни с RED R01–R05, затем реализуй exact
> need aggregates и фактическую трассу выбора с no-command, разделением scopes и
> причинами неоценённых вариантов. Не меняй физический порядок расчёта, не считай
> сохранённые Goal/Plan уже работающим генератором. Выполни CTest и предъяви T2 отдельно.
