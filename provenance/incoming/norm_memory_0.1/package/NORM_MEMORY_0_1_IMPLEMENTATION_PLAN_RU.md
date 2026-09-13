# NORM-MEMORY-0.1 — подробный план реализации

> Для исполнителя: применять task-by-task workflow из `superpowers:executing-plans` либо `superpowers:subagent-driven-development`; TDD для новых функций и регрессий. Документ — план работ, а не список уже сделанных изменений.

**Цель:** получить объяснимый выбор NPC из собственных усвоенных норм и ожиданий, а не из глобальной команды «сделай как все»; сохранить SELF-0.1 и работающие цепочки RECOVERY 0.13.  
**Архитектура:** типизированные проекции → оплаченная интерпретация → NormMemory → ограниченный NormDecisionView → известные способы/вопросы → единый ledger последствий → действие. Личное принятие принципа, описательные практики и прогноз реакции аудитории не сливаются.  
**Стек:** существующие C++20, CMake ≥3.20, собственный `TEST/CHECK/NEAR/THROWS`, Python standard library для анализа. Новых внешних библиотек не требуется.  
**Спецификация:** `NORM_MEMORY_0_1_SPEC_RU.md`, устанавливается в `docs/rules/norm_memory_0.1/SPEC_RU.md`.  
**База main:** `36dc381b121e552fd4c961d8a2a90fd48a2a09d0`.  
**Установка плана:** `docs/superpowers/plans/2026-09-13-norm-memory-0.1.md`.

## Общие ограничения

- Новая ветка от проверенной RECOVERY 0.13. Не начинать от исторической SELF 0.12 или COMMUNITY 0.10.
- Не редактировать legacy/reference как способ заставить активные тесты пройти.
- World не сообщает NPC скрытые мнения, актуальные чужие координаты, кошельки или нормы группы.
- Executor не вызывает SelfModel::integrate и не назначает новые психологические коэффициенты.
- Ни факт большинства, ни дефицит потребности не гарантируют конкретного выбора.
- Цена морального запрета конечна; физическое участие/согласие другого проверяется исполнителем независимо.
- Нет новых бесплатных вычислительных бюджетов, глобального рейтинга статуса, автоматических штрафов или «conformism».
- Исследование чужой вещи не превращается в собственный социальный успех.
- Каждая операция/сообщение/эффект различает техническую повторную доставку и новое смысловое основание.
- Все формулы и новые численные параметры находятся в SPEC и PROFILE с версией; не распределять новые magic numbers по World.
- Сам план не даёт права объявлять проведёнными его будущие тесты. Состояние шага меняется только по журналу команды.

## 1. Измеримые цели

| ID | Цель | Доказательство |
|---|---|---|
| G1 | Условия/практики становятся личной памятью | Два мира с одинаковой проекцией и разной скрытой политикой дают одинаковые личные обновления |
| G2 | Норма влияет через выбор | Источник → изменение belief → вопрос → альтернативы → выбор → настоящий результат, без прямого start_action из сообщения |
| G3 | Не теряется личная мораль | Высокий личный принцип действует в отсутствии аудитории; частота нарушения не изменяет w автоматически |
| G4 | Не возникает двойного наказания | Один ConsequenceKey даёт одну статью результата при Q+Norm+Self+проекте |
| G5 | Известные процедуры используются как способы | Новая норма может активировать известный способ; неизвестный не материализуется |
| G6 | Самостоятельность и ложные убеждения | Наблюдатель/получатель могут ошибаться; адресат независимо отвечает |
| G7 | Сохранность SelfModel | Все существующие self-тесты плюс парная проверка новых нормативных опытов |
| G8 | Нет блокирующего ожидания | Неисполняемый нормативный проект ждёт основание, но не захватывает основное действие и все мыслительные обзоры |
| G9 | Честная проверка поведения | Отдельно контрольные ситуации, свободные миры, стресс-профиль, происхождение каждого бинарника |

## 2. Решения до начала кода

### D1. Где хранить нормы

Выбран самостоятельный `Mind::norm_memory`, на одном уровне с SelfModel и знаниями. Не создавать NormMemory внутри Body и не копировать её целиком в Planner. SocialMemory продолжает хранить адресный опыт, а общий групповой прогноз находится в NormMemory.

### D2. Что сделать со старыми числами

Сохранить Legacy для regressions. В Enabled личные `norms[]` импортируются в `PersonalPrinciple` с provenance `LegacyPrior`. Внешние `public_*_disapproval` становятся явными исходными ожиданиями аудитории. Для одного компонента запрещено одновременное применение старого массива и нового прогноза.

### D3. Что считать новой памятью

Технический delivery повторно не обрабатывается. Семантическая revision известного основания может заменить прежний вклад. Скрытый graph/root не используется для установления известной независимости слухов.

### D4. Как избежать произвольного желания быть как все

Описательная практика предлагает вопрос/способ. Мотив связан с существующей личной целью и значимостью известной группы. SelfModel.Acceptance — не желание принадлежать. Цена/польза появляется от прогнозируемого результата или принятого принципа.

### D5. Что делать с бюджетом

`GuardedBaseline` сохраняет прежний запрет тратить резерв. `Deliberative` добавляется отдельным профилем с явной ценой ухудшения буфера и без гарантии безопасности решения. Не балансировать оба режима одним набором ожиданий выживания.

### D6. Что не строить попутно

Не строить полное правосудие, свободную языковую семантику, новую модель всех групп, новые звонки, промышленность или произвольное планирование. Достаточно замкнутых цепочек одежды, обещаний, приватности, помощи и конечных социальных реплик.

## 3. Карта файлов

### Новые файлы

| Путь | Ответственность |
|---|---|
| `engine/include/life/norm_types.hpp` | Содержательные ключи, источники, наблюдения, прогнозы; без зависимости от World |
| `engine/include/life/norm_memory.hpp` | Контракты хранения, расчёта и ревизии |
| `engine/src/life/norm_memory.cpp` | Сводки, дедупликация известного происхождения, decay, revision |
| `engine/src/life/norm_world.cpp` | Получение доступных проекций, публикация inbox, начальная история |
| `engine/src/life/norm_cognition.cpp` | Оплаченные операции интерпретации/рассмотрения |
| `engine/src/life/norm_social.cpp` | Payload/объяснение/реакция/вопрос о практике, интеграция телефона |
| `engine/include/life/decision_ledger.hpp` | Контракт ConsequenceKey и raw-статей |
| `engine/src/life/decision_ledger.cpp` | Объединение одного исхода, временное дисконтирование, итоговая оценка |
| `engine/src/life/norm_diagnostics.cpp` | Только журнал и экспорт для разработчика |
| `engine/tests/life/test_norm_memory.cpp` | Математика и хранение |
| `engine/tests/life/test_norm_runtime.cpp` | World/cognition/physical boundary |
| `engine/tests/life/test_norm_choices.cpp` | Доказуемое изменение альтернатив и выбора |
| `engine/tests/life/test_norm_persistence.cpp` | Сохранения, порядок, потоки, кэш |
| `engine/tests/life/norm_fixtures.hpp` | Общие узкие фикстуры с явным происхождением |
| `tools/norm_scenarios.cpp` | Сквозные авторские сценарии, не production-назначение выбора |
| `tools/run_norm_matrix.py` | Пакетные запуски и SHA/returncode |
| `tools/analyse_norm_matrix.py` | Анализ метрик, пар и причин |
| `game/profiles/norm_memory_0.1/PROFILE.json` | Параметры, режимы, нагрузки, нормативное содержание |
| `docs/rules/norm_memory_0.1/SPEC_RU.md` | Утверждённые правила |
| `docs/architecture/norm_memory_0.1/IMPLEMENTATION_STATUS.md` | Требование → код → тест → ограничение |

### Существующие точки изменения

`mind.hpp/.cpp`, `social.hpp/.cpp`, `world.hpp/.cpp`, `cognition.hpp/.cpp`, `cognitive_world.cpp`, `civil.hpp/.cpp`, `civil_world.cpp`, `civil_social_world.cpp`, `phone_world.cpp`, `project_world.cpp`, `projects.hpp/.cpp`, `executor.cpp`, `generation.cpp`, `validation.cpp`, `persistence.cpp`, `worker.cpp`, `social_diagnostics.cpp`, `apps/simulate/life_main.cpp`, `CMakeLists.txt`, `navigation/INDEX.md`, `navigation/code-map.json`, `navigation/doc-map.json`.

Не создавать второй `planner.cpp`: актуальные `Planner::ideas/forecast` находятся в `engine/src/life/mind.cpp`. Не путать архивные пути с путями main.

## 4. Контракт интерфейсов будущего кода

Ниже **проект API**, а не существующие символы. Этот блок является общим договором задач S1–S9. Имена не следует по-разному выдумывать в отдельных подзадачах.

```cpp
// norm_types.hpp: только value-types; Id и Tick из semantic_types.hpp.
enum class NormMode : std::uint8_t {
    Legacy, Shadow, Enabled, FrozenLearning, NoNormDecisionEffects
};
enum class NormChannel : std::uint8_t {
    Descriptive, Approval, Detection, Classification, Reaction, PersonalPrinciple
};
enum class NormOrigin : std::uint8_t { Observed, Reported, Historical, Assumed, LegacyPrior };
enum class ApprovalValue : std::uint8_t { Approve, Disapprove, Indifferent };
enum class ApplyEvidence : std::uint8_t { Added, Replaced, Duplicate, Unknown, Rejected };
struct NormKey {
    std::uint32_t practice{}, local_group{}, context{}, actor_role{}, variant{};
    auto operator<=>(const NormKey&) const = default;
};
struct NormSource {
    std::uint64_t delivery{}, known_root{}, revision{};
    Id speaker{}; NormOrigin origin{};
    bool independently_grounded{};
    // world_root отсутствует: это не знание персонажа.
};
struct NormObservation {
    NormKey key{}; NormSource source{};
    Tick at{}; Id observed_actor{};
    double quality{}, confidence{}, reliability{1}, dose{};
    double value{}; NormChannel channel{};
    ApprovalValue approval{};
    bool applicable{}, value_known{}, outcome_window_complete{};
};
struct BinaryNormEstimate {
    double positive{}, negative{}; // достаточно для чистой математики;
    double prior_positive{1}, prior_negative{1};
    bool has_explicit_prior{};
    bool known() const;
    double probability() const;
    double coverage() const;
};
struct ApprovalEstimate {
    std::array<double,3> evidence{};
    std::array<double,3> prior{1,1,1};
    bool has_explicit_prior{};
    bool known() const;
    std::array<double,3> probabilities() const;
};
struct NormPrediction {
    NormKey key{};
    bool descriptive_known{}, approval_known{}, sanction_known{};
    double prevalence{}, coverage{}, approve{}, disapprove{}, indifferent{};
    double seen{}, classified{}, reacted{}, severity{};
    double personal_resistance{};
    std::uint64_t own_revision{};
};
struct NormDecisionView {
    NormMode mode{NormMode::Legacy};
    std::array<NormPrediction,4> considered{};
    std::uint8_t count{};
    // Нет сырых наблюдений, скрытых root или всех членов группы.
};
```

Обязательные функции: `NormMemory::apply(const NormObservation&, Tick) -> ApplyEvidence`; `NormMemory::predict(const NormKey&, Tick) const -> NormPrediction`; `NormMemory::validate(Tick) const`; `NormMemory::revision() const -> uint64_t`. Хранилище имеет сериализуемые сводки/источники и явные операции пересмотра. Прогноз не должен вызывать `apply`.

```cpp
// decision_ledger.hpp: producer — владелец математического результата.
enum class ConsequenceKind : std::uint8_t {
    BodilyRelief, Enjoyment, MaterialAccess, SocialAcceptance,
    ExternalSanction, PersonalPrinciple, Time, Resource,
    Switching, ResidualUncertainty
};
enum class ForecastOwner : std::uint8_t { DirectExperience, NormPrior, SelfPrior, Procedure, PhysicalModel };
struct ConsequenceKey {
    ConsequenceKind kind{}; Id target{}, object{}; std::uint64_t horizon{};
    auto operator<=>(const ConsequenceKey&) const = default;
};
struct ConsequenceTerm {
    ConsequenceKey key{}; ForecastOwner owner{};
    double amount{}, probability{1}, time_hours{};
    bool present_value{};
};
class DecisionLedger {
public:
    void insert_unique(const ConsequenceTerm& term); // duplicate key: error, not sum
    double present_value(double discount_per_hour) const;
    double score(double discount_per_hour) const;
};
```

Слияние разных прогнозов одного исхода выполняется **до** `insert_unique` по формулам SPEC §10. Не выбирать автоматически производителя по большему reward. Политика: конкретный известный опыт → контекстный norm-prior → слабый self-prior; ресурс и время — отдельные физически осмысленные цены.

World API будущего кода: `configure_norm_memory(const NormProfile&)`, `publish_norm_observation(Actor&, const NormObservation&)`, `build_norm_decision_view(const Actor&, const NormContextSnapshot&)`, `norm_candidates(Actor&)`, `begin_norm_operation(Actor&)`, `complete_norm_operation(Actor&, Operation)`, `validate_norm_state() const`, `export_norm_state(std::ostream&) const`.

Дополнительные контракты, чтобы сигнатуры выше не ссылались на неопределённое содержание:

```cpp
struct NormProfile {
    NormMode mode{NormMode::Legacy};
    double half_life_days{30}, coverage_kappa{4};
    double personal_learning_factor{.02}, motive_on{.12}, motive_off{.07};
    std::uint32_t inbox_limit{32}, hot_record_limit{128};
    std::uint32_t candidate_ids_limit{32}, deep_norm_limit{4};
    std::uint32_t group_limit{4}, new_option_limit{2};
    void validate() const;
};
struct NormContextSnapshot {
    Tick now{}; std::uint64_t own_revision{};
    std::array<NormKey,4> selected_keys{};
    std::array<double,4> goal_relevance{};
    std::uint8_t count{};
    // Снимок уже оплаченного выбора контекста, не весь реестр групп.
};
```

Полный профиль данных — `NORM_MEMORY_0_1_PROFILE.json`. Ресурсный `budget_policy` живёт в CivilProfile, не внутри NormMemory. Если работа ведётся через JSON-loader, создавать валидатор этих полей; наличие JSON рядом само по себе не означает, что C++ его читает. Флаги CLI: `legacy|shadow|enabled|frozen-learning|no-effects`, отображаются в enum по явной таблице.

Новые операции должны расширять существующий enum **в конце**: `InterpretNormObservation`, `IntegrateNormEvidence`, `RecallNormContext`, `CompareNormAlternatives`, `ReflectPersonalPrinciple`. Аналогично новые Interaction из SPEC §13. Их формат сохранения всё равно версионируется явно.

## S0. Зафиксировать исходную базу и регрессии

**Цель:** исключить повторную реализацию поверх старой ветки, неверное применение архива и приписывание старых тестов новой сборке.  
**Зависимости:** нет.  
**Файлы:** текущий main, `provenance/`, создаваемая `docs/architecture/norm_memory_0.1/BASELINE.json`.

- [ ] Получить актуальный `git status`, HEAD и список source-файлов. Создать изолированную ветку `feature/norm-memory-0.1` от подтверждённой RECOVERY. При изменившемся HEAD сначала сравнить изменения, а не force-reset main.
- [ ] Проверить существование новых recovery source-файлов и точные пути. Сопоставить blob/SHA с входным архивом для выбранных точек интеграции.
- [ ] Выполнить baseline:

```sh
cmake -S . -B build-norm-base -DCMAKE_BUILD_TYPE=Release
cmake --build build-norm-base --parallel 2
ctest --test-dir build-norm-base --output-on-failure
./build-norm-base/life_tests
python tools/verify_navigation.py
python -m unittest tools.tests.test_verify_navigation -v
```

- [ ] Записать команды, версии компилятора, returncode, source hash, CLI hash, полный список зарегистрированных CTest. Не использовать заявленные исторические 445/29 как свежие результаты.
- [ ] Снять один baseline `--recovery --population 16 --seed 42 --days 1` с summary/self/career/recovery/thoughts в новый каталог; это reference для сквозной диагностики, не физический стандарт поведения.
- [ ] Зафиксировать SPEC/PLAN с непоставленными ещё компонентами `planned`.

**Выход:** идентифицированная база и сохранённые baseline-логи. Красный старый тест сначала диагностируется; его нельзя переименовать в «новый баланс».  
**Commit:** `docs(norms): pin recovery baseline and normative contracts`.

## S1. Типы, neutral state и точное разделение личного/группового

**Цель:** ввести состояние без влияния на поведение.  
**Файлы:** `norm_types.hpp`, `norm_memory.hpp/.cpp`, `mind.hpp`, `cognition.hpp`, `test_norm_memory.cpp`, CMake.

- [ ] Написать тесты NM-001..NM-005 на neutral/unknown, диапазоны и отсутствие входов World.
- [ ] Добавить минимальные типы из §4; нейтральный technical prior не выдаётся за знание.

```cpp
TEST("norm_memory", no_evidence_is_unknown_not_social_permission) {
    BinaryNormEstimate x;
    CHECK(!x.known());
    NEAR(x.probability(), .5, 1e-12);
    NEAR(x.coverage(), 0, 1e-12);
}
TEST("norm_memory", categorical_approval_has_a_separate_neutral_outcome) {
    ApprovalEstimate x;
    x.evidence = {5,2,1};
    auto p = x.probabilities();
    NEAR(p[0], 6./11, 1e-12);
    NEAR(p[1], 3./11, 1e-12);
    NEAR(p[2], 2./11, 1e-12);
}
```

- [ ] Увидеть ожидаемый RED после появления минимального нейтрального API; не считать ошибку include доказательством поведения.
- [ ] Реализовать валидацию finite, неотрицательность весов, положительность prior, диапазоны enum/time. Неверные аргументы отвергать до изменения объекта.
- [ ] Добавить `Mind::norm_memory`, `CognitiveState` поля inbox/снимка, `PersonalView::norms_view`; в Legacy count=0 и нет влияния.
- [ ] Прогнать новый набор и существующие self/phone/civil/projects. Подтвердить равенство **проекции старого поведения**, не полного нового state blob.

**Выход:** сериализуемые типы и нейтральный legacy-compatible слой.  
**Commit:** `feat(norms): add isolated belief types and neutral state`.

## S2. Свидетельства, частоты, забывание и пересмотр

**Цель:** обучать содержание нормы, не количество доставленных копий.  
**Файлы:** `norm_memory.cpp`, `test_norm_memory.cpp`, `norm_fixtures.hpp`; SPEC §§5–7.

- [ ] Написать NM-006..NM-018 и увидеть RED: дубликат, новая revision, слабое качество, исчезновение известности, молчание, повтор взгляда.
- [ ] Реализовать вес q и binary/categorical updates; uncertainty и assumed prior не превращать в факты.

```cpp
TEST("norm_memory", eight_of_ten_has_finite_uncertain_prediction) {
    BinaryNormEstimate x;
    x.positive=8; x.negative=2;
    NEAR(x.probability(), .75, 1e-12);
    NEAR(x.coverage(), 10./14, 1e-12);
}
```

- [ ] Для каждой записи хранить ключ собственной независимости, вклад, semantic_revision и время основания. `apply` возвращает Added/Replaced/Duplicate/Unknown/Rejected.
- [ ] Реализовать decay в одной временной системе и корректное вычитание старого вклада при revision. При ошибке входа — strong exception guarantee: сводка не изменена.
- [ ] Разделить индекс delivery и граф **известного** происхождения. Не помещать world_root в прогноз.
- [ ] Добавить исторический prior и долгосрочное хранение; отрисовка/чтение не меняет источник или дозу.
- [ ] Проверить timestep splitting, повтор чтения и иное представление известного общего источника.

**Выход:** чистый NormMemory с исправляемыми собственными убеждениями.  
**Commit:** `feat(norms): implement evidence accounting and revision-safe estimates`.

## S3. Собственное восприятие и оплаченная интеграция

**Цель:** единственный runtime-путь формирования групповых убеждений через cognition.  
**Файлы:** `norm_world.cpp`, `norm_cognition.cpp`, `world.hpp`, `cognition.hpp/.cpp`, `cognitive_world.cpp`, `worker.cpp`, runtime-тесты.

- [ ] RED NM-019..NM-031: скрытое событие, gate=0, автономия выключена, результат до/после завершения операции.
- [ ] Получать доступные данные одежды из прежнего сенсорного контура. Наблюдение одной вещи в 100 кадрах не создаёт 100 участников.
- [ ] `publish_norm_observation` только ставит ограниченную запись, invalidates собственную situation revision и однократно будит внимание. Не вызывает `NormMemory::apply`.
- [ ] Начать две обычные операции; при budget<2 сохранять ожидание без изменения belief.
- [ ] При завершении интеграции сопоставить захваченные own_revision/source_revision; stale операция отменяется/пересматривается без частичного результата.
- [ ] Чужое поведение не публикует собственный Self-успех. Личный ответ/реакция при действительном общении идёт по прежнему SELF-пути.

Требуемая интеграционная проверка:

```text
Исходно: нейтральная NormMemory; автономия выключена.
Видимое действие другого завершено → в inbox есть NormObservation, NormMemory прежняя.
Автономия включена → InterpretNormObservation завершена, NormMemory ещё прежняя.
IntegrateNormEvidence завершена → изменён ровно один подходящий NormRecord.
SelfModel наблюдателя не получил фиктивного собственного социального успеха.
```

- [ ] Повторить с невидимой вещью, нераспознанной группой и неполным исходом реакции.

**Выход:** обучающее событие можно проследить от факта до собственных данных.  
**Commit:** `feat(norms): integrate perception through paid cognitive operations`.

## S4. Общение о нормах и самостоятельные реакции

**Цель:** NPC может узнать/обсудить правило и реально одобрить/осудить конкретный поступок.  
**Файлы:** `social.hpp/.cpp`, `social_world.cpp`, `norm_social.cpp`, `phone_world.cpp`, `civil_social_world.cpp`, `activity_resources.cpp`, тесты.

- [ ] RED NM-032..NM-041: рассказ не равен наблюдению, слушатель не слышит/отказывает, телефон не обработан, скрытый источник не раскрыт.
- [ ] Добавить четыре Interaction из SPEC §13. Каталог определяет типизированное содержание, длительность, ресурсы; не предписывает всем делать реакцию.
- [ ] В payload хранить NormKey, высказывание/исход, известное происхождение и уверенность говорящего. Не передавать сырой NormMemory или true-world-policy.
- [ ] Кандидат реакции создаётся только по доступному поступку и известному способу. Собственный принцип/цель придают значимость; чужое действие не запускает `propose_social` напрямую.
- [ ] Один реальный акт осуждения создаёт один социальный OutcomeSignal и одну проекцию о норме; получатели обрабатывают по своим gate/времени.
- [ ] Подключить тот же payload к отдельным отправке/доставке/чтению/обработке текста.
- [ ] Отсутствие реакции до конца окна не превращать в «одобрено». Для внешних институтов не генерировать штрафы.

**Выход:** источник нормы — настоящий наблюдённый/полученный акт, а не инъекция глобального числа.  
**Commit:** `feat(norms): add typed norm dialogue and voluntary social feedback`.

## S5. Личные принципы и безопасная миграция старых норм

**Цель:** сохранить внутреннюю мораль и убрать двойное применение старого/нового.
**Файлы:** `norm_memory.cpp`, `generation.cpp`, `mind.hpp/.cpp`, `social.cpp`, `executor.cpp`, `test_norm_memory.cpp`, `test_norm_choices.cpp`.

Таблица миграции:

| Старый компонент | Новый владелец |
|---|---|
| `Mind.norms[TakeFood]` | Личный принцип распоряжения чужим; условия по личным owner/permission |
| `Mind.norms[PrivateIntimacy]` | Личный prior отношения к соответствующему способу; не физиология |
| `SocialMemory.norms[Property/Honesty/Privacy/Promise]` | PersonalPrinciple c контекстной квалификацией |
| `Boundary/PersonalRomance/Modesty` | Личные принципы соответствующего содержания; не универсальная культура |
| `public_*_disapproval` | Явные prior прогноза реакции аудитории |
| `Mind.risks[TakeFood]` | Legacy ожидание санкции с provenance; не вторая цена после NormPrediction |
| `community.confidentiality/stranger_romance/kin_romance` | В Enabled квалифицируемые личные аспекты/ожидания; правило владельца обязательно |
| Приятность, предпочтения, болезненность отказа | Не переносить автоматически в нормы: сохраняют свой смысл |

- [ ] RED NM-042..NM-049: w=1 конечно; отсутствие свидетеля; неизвестное разрешение; оправдывающее обстоятельство; пассивная частота не меняет w.
- [ ] Импортировать seed-числа как `LegacyPrior`, не генерировать вымышленные события для красивой биографии.
- [ ] Реализовать `ReflectPersonalPrinciple` с формулой МОР-0.2. Применение нового исключения не ослабляет сам принцип автоматически.
- [ ] Сделать режим взаимоисключающим: Enabled читает только нового владельца; Legacy — прежние значения. Проверка NaN/скрытого второго массива должна обнаруживать несанкционированное чтение.
- [ ] Перенести смысл квалификации поступка из физического executor в собственную интерпретацию; физические эффекты и право участника отказать не менять.

**Выход:** внутренняя мораль не исчезла и не считается дважды.  
**Commit:** `refactor(norms): migrate legacy components into explicit personal and social beliefs`.

## S6. Единый прогноз и отсутствие двойной награды/цены

**Цель:** NormMemory меняет прогноз конкретного результата, а не умножает Score.
**Файлы:** `decision_ledger.hpp/.cpp`, `mind.cpp`, `social.cpp`, `project_world.cpp`, `norm_cognition.cpp`, тесты.

- [ ] RED NM-050..NM-059: повтор результата, Self+Norm+Q, raw/squash, project discount, один адресат в двух группах.
- [ ] Добавить raw ledger для Enabled. Каждый результат вставляется с уникальным ConsequenceKey.

```cpp
TEST("norm_choices", duplicate_consequence_is_not_a_second_penalty) {
    DecisionLedger x;
    ConsequenceTerm t{{ConsequenceKind::SocialAcceptance,2,0,1},
                      ForecastOwner::NormPrior,-.2,1,0,false};
    x.insert_unique(t);
    THROWS(x.insert_unique(t));
    NEAR(x.present_value(.03), -.2, 1e-12);
}
```

- [ ] Объединить person-specific/Q/norm/self-прогноз до вставки одного социального исхода. При конфликте владельцев диагностическая ошибка вместо молчаливого удвоения.
- [ ] Точную старую функцию self_cost не заменять новыми «смелостью/конформностью». Применить к остаточной неопределённости и оставить историю SelfModel неизменной при forecast.
- [ ] Учесть время результата один раз: уже present_value continuation не получает повторный exp-discount. Одобрение от будущего появления в новой одежде не равно немедленной награде за покупку.
- [ ] Сравнить кандидата с продолжением через существующий assess_interruption. NormTension не разрешает бросить работу ради любой альтернативы.
- [ ] Сохранить отдельные traces нового raw-ledger и legacy-score для диагностики; не сравнивать нормированные и ненормированные числа.

**Выход:** арифметику каждого выбора можно воспроизвести из одного конечного списка результатов.  
**Commit:** `feat(norms): evaluate distinct consequences in a single decision ledger`.

## S7. Мотивы, известные способы, одежда и бюджет

**Цель:** реальные альтернативы доступны до оценки; норма не заставляет выполнять заранее выбранный путь.
**Файлы:** `civil.hpp/.cpp`, `civil_world.cpp`, `project_world.cpp`, `projects.hpp/.cpp`, `mind.cpp`, `deliberation.cpp`, `norm_cognition.cpp`.

- [ ] RED NM-060..NM-070: выбор нескольких SKU, необходимость vs престиж, деньги только на базовую вещь, ожидание не блокирует, условие возобновления.
- [ ] Добавить нормативный вопрос, on/off=.12/.07. Ключ по практике/группе/контексту/цели; одинаковая revision не создаёт новые экземпляры.
- [ ] Генерировать способы из известных процедур и доступного содержания. До двух новых предложений; общий максимум 8 и жизненные remedies сохраняются.
- [ ] В Enabled не отбрасывать все известные SKU, отличающиеся от desired_tier. Сохранение одежды/запрос условий — настоящие альтернативы.
- [ ] Для рабочего появления трактовать одежду как средство; сама покупка не увеличивает автоматически принятие или SelfModel.
- [ ] Добавить два бюджетных режима. Deliberative использует SPEC §12 и журнал принятого риска, Guarded сохраняет прежнюю защиту.

```text
RED-сценарий:
  Состояние одежды .9; известны SKU 0/35 и 1/95; деньги=220, резерв=100.
  Варианты до нормы: сохранить вещь, приобрести 0, приобрести 1, уточнить.
  Норма именно «исправная одежда» → покупка 1 не получает обязательного бонуса цены.
  Иное доступное основание «категория 1 значима этой группе» может изменить оценку,
  но не должно удалить остальные варианты.
```

- [ ] Недостаток денег оставляет вопрос накопления/известной помощи; не запускает бесконечную покупку и не переводит деньги из скрытого будущего.
- [ ] Проверить спонтанную доступную встречу: обсуждение нормы не требует заранее назначить свидание/встречу.

**Выход:** сценарии действительно проверяют выбор, а не один остаточный вариант после фильтрации.
**Commit:** `feat(norms): activate contextual goals and compare affordable known strategies`.

## S8. История, группы, конфликты и переобучение

**Цель:** разные собственные истории дают разные нормы без новых магических черт.
**Файлы:** `generation.cpp`, `norm_world.cpp`, `norm_memory.cpp`, PROFILE, `norm_scenarios.cpp`.

- [ ] RED NM-071..NM-078: другая группа, несколько ролей одного человека, скрытая смена членства, собственный ложный вывод.
- [ ] Сформировать 3–4 явно авторских проверочных контекста: рабочая исправность одежды, престижная компания, экономный досуг, конфиденциальное общение.
- [ ] Создать доступные эпизоды/сообщения, не устанавливать posterior произвольно. Prior, усвоенный рассказ и личное наблюдение экспортируются раздельно.
- [ ] История «многие делают X» меняет описательный слой, не автоматически одобрение и не личный принцип.
- [ ] При смене работы старые нормы не удаляются; новая группа имеет собственный контекст и приоритет через цели.
- [ ] Проверить возможность ложного ожидания: NPC видит малую выборку и делает помеченное усвоенное обобщение; симулятор не исправляет его истинной статистикой населения.

**Выход:** поведение объясняется источниками, а не ярлыком «такой характер».  
**Commit:** `feat(norms): seed interpretable group histories and scope-aware learning`.

## S9. Сохранения, кэш, конкурентность и защита от зацикливания

**Цель:** новые мыслительные операции воспроизводимы, не увеличивают неоплаченный бюджет и не повреждают данные.
**Файлы:** `persistence.cpp`, `validation.cpp`, `worker.cpp`, новые norm-файлы, тесты persistence.

- [ ] RED NM-079..NM-087: сохранение посередине каждой операции и текста; cold/hot; порядок; логирование; перегрузка.
- [ ] Повысить формат до `LIFE-SAVE-0.14.0-norm01-r1`, старый отвергать явным сообщением. README и CLI должны сообщать тот же формат.
- [ ] Сериализовать active snapshot, pending revision, ссылки вопросов, источники и стадийность сообщения. Не потерять in-flight личный Self-эпизод.
- [ ] Инвалидация только от собственного знания, цели, доступного контекста и времени. Скрытые перемены политики группы её не вызывают.
- [ ] Переполнение inbox не выдаёт фиктивное «усвоено»: deferred/dropped counters с причиной. Горячий лимит не удаляет холодное убеждение.
- [ ] Проверить 1/2/4 workers и input/logging permutations. Время события и canonical tie-break задают порядок, а не порядок завершения потоков.
- [ ] Повторный обзор ожидания через 30 секунд допустим; новый NormUpdate без source/revision не допустим. Нулевая длительность бесконечной попытки — failure.

**Выход:** новые подсистемы выдерживают прерывание, restore и детерминированное исполнение.
**Commit:** `fix(norms): make evidence and cognitive progress replay-safe`.

## S10. Приёмочная матрица, свободные миры и измерения

**Цель:** отделить корректность формулы, исполнение цепочки и самопроизвольный выбор.
**Файлы:** `norm_scenarios.cpp`, `run_norm_matrix.py`, `analyse_norm_matrix.py`, тесты, PROFILE, CMake, CI.

- [ ] Добавить executable `norm_scenarios`. Сценарии используют реальные World/Planner/сообщения, не заменяют production `choose` фейковой функцией.
- [ ] Добавить groups CTest: `norm_memory`, `norm_runtime`, `norm_choices`, `norm_persistence`, `norm_scenarios`, `norm_long`. `norm_long` пометить `long`; исключения санитайзеров перечислять явно.
- [ ] Выполнить C++ regression + все новые тесты матрицы (не менее NM-001..NM-087). Число берётся из регистрации, а не из строк документа.
- [ ] Проверить причинные пары одежды, приватности, возврата, помощи и работы. Содержание истории меняется в одной копии, действительность одинакова.
- [ ] Свободный первоначальный набор: seeds 42,7,101,2026; 16 взрослых; 14 игровых суток; четыре режима Legacy/Enabled/NoNormDecisionEffects/FrozenLearning; GuardedBaseline. Это 16 миров, не один удачный seed.
- [ ] Для явно ресурсного конфликта отдельно 4 Deliberative мира с теми же seed; выводы не смешивать с безопасным контролем.
- [ ] Расширение после прохождения: 32 seed для основных парных сцен; 128 NPC×7 суток для нагрузки; ни один результат не называть benchmark i5/UE5 без нужной платформы.

Команды для будущей реализации CLI (флаги должны быть реализованы в этой задаче):

```sh
./build-norm/life_sim --recovery --norm-memory enabled --budget-policy guarded \
  --population 16 --seed 42 --days 14 \
  --summary run/summary.json --self-state run/self.json \
  --career-state run/career.json --recovery-state run/recovery.json \
  --norm-state run/norms.json --norm-trace run/norms.jsonl \
  --thoughts run/thoughts.jsonl --save run/final.save

./build-norm/norm_scenarios --case clothing_history_pair --seed 42 --out run/pair
```

Новые флаги `--norm-memory`, `--budget-policy`, `--norm-state`, `--norm-trace` **не существуют в 0.13**; их код, help, invalid-argument tests — часть S10. Остальные параметры сверить с реальным CLI, не предполагать орфографию по примеру.

### Обязательные метрики

- Число наблюдённых возможностей; сколько распознано/интерпретировано/усвоено/отложено/отброшено.
- Distinct delivery, known roots, revisions, суммарная доза; коэффициент дублирования.
- NormRecord по группам/каналам; confidence/coverage; доля Unknown и Conflicting.
- Возвращённые вопросы по новому факту, таймеру и повтору; последний должен быть нулём для одинаковой revision.
- Предложенные/извлечённые/оценённые/вытесненные кандидаты с причинами.
- Выбранные дорогие/базовые вещи и сохранения одежды; не только покупки.
- Просьбы, ответы, согласия, завершённые передачи; сумма денег до/после.
- Начатые/завершённые/прерванные действия; отказ адресата не смешивать с физическим fail.
- Основной бюджет суток ровно 24 ч/NPC; вторичные коммуникации — отдельное перекрытие.
- Критический голод/жажда/сон по отдельности и объединение интервалов, максимум эпизода и причина.
- SelfUpdate count/root/domain; отсутствие собственного Acceptance от чужого наблюдения.
- CPU/p50/p95 операции решения, объём собственного state/cold sources, число глубоко применённых норм.

### Правило интерпретации

В арифметическом тесте при зафиксированных остальных слагаемых может требоваться монотонное изменение U. В свободном мире не требовать монотонности всех поступков: разные маршруты меняют дальнейший опыт. Считать парные изменения по одинаковым возможностям и отдельно свободные траектории.

Для конкретной истории с заданным выигрышем >switch_margin требовать смену выбора. Для неоднозначного случая допустим неизменный выбор при изменившейся оценке — trace должен объяснить, какая более сильная цель сохранила решение.

Не выбирать статистический порог постфактум. В парной серии заранее сохранить число b (только опытная копия выполнила действие), c (только контрольная), n=b+c; использовать двусторонний точный знаковый тест с p по Binomial(n,.5), эффект и интервал по парам. Это анализ модели, не психологическая валидация людей. Несколько сценариев требуют заранее выбранной основной метрики и раздельного отчёта.

**Выход:** отчёт о реальных причинных цепочках и ограничениях.  
**Commit:** `test(norms): validate causal choices and multi-seed stability`.

## S11. Проверка требований и поставка

**Цель:** передать существующий проверенный код, а не архив из отчётов.
**Файлы:** README, navigation, docs architecture/report, provenance, CI, release package.

- [ ] Просмотреть diff независимо от автора изменений либо отдельным review-агентом при наличии. Проверить каждый риск из §6 этого плана.
- [ ] В `IMPLEMENTATION_STATUS.md` для G1–G9 и каждого обязательного требования указать code/test/result; явно оставить `not implemented`, если что-то не готово.
- [ ] Перезапустить полный Release CTest; отдельно sanitizer suite с явными исключениями.

```sh
cmake -S . -B build-norm -DCMAKE_BUILD_TYPE=Release
cmake --build build-norm --parallel 2
ctest --test-dir build-norm --output-on-failure
./build-norm/life_tests
cmake -S . -B build-norm-asan -DCMAKE_BUILD_TYPE=Debug -DLIFE_SANITIZERS=ON
cmake --build build-norm-asan --parallel 2
ctest --test-dir build-norm-asan --output-on-failure -LE long
python tools/verify_navigation.py
python -m unittest tools.tests.test_verify_navigation -v
```

`-LE long` исключает только фактически помеченные задания; проверить `ctest -N -V`, не предполагать автоматически, что world помечен. Итоговый отчёт перечисляет все реально исключённые имена.

- [ ] Сформировать patch именно от зафиксированной базы. Временно распаковать чистую базу, выполнить `git apply --check`, применить, сверить SHA каждого файла.
- [ ] Создать архив с CMake, headers, source, tests, spec, профилем и evidence. Проверить Zip CRC и отсутствие случайных абсолютных путей.
- [ ] Распаковать новый архив в другой каталог, собрать и запустить CTest; проверить совпадение входных source hashes.
- [ ] Каждый окончательный эксперимент должен иметь source/CLI/profile/initial-state hashes, команду, returncode и логи. Старые неуспешные итерации хранить как diagnostic, не как final.
- [ ] Обновить версии README/CMake/save/catalogue/profile/source manifest согласованно. Не оставлять только имя архива с новой версией.
- [ ] Запись в GitHub выполняется отдельным разрешённым действием; без фактического push не утверждать, что main обновлён.

**Выход:** новый patch+source+evidence, пригодный для повторения.  
**Commit:** `release(norms): package verified source and traceable evidence`.

## 5. Зависимости и возможная параллельная работа

```text
S0 → S1 → S2 → S3 → S4
              └→ S5 → S6 → S7
S3+S4+S7 → S8 → S9 → S10 → S11
```

S4 и S5 можно разрабатывать параллельно только после фиксации общих enum/payload contracts. Владелец изменений `mind.hpp`, `social.hpp`, `cognition.hpp`, CMake и serialization — один интегратор; не отдавать эти файлы нескольким исполнителям одновременно. Тесты/анализаторы можно готовить отдельно, не подменяя модели mocks.

## 6. Реестр вероятных багов и их обнаружение

| Риск | Почему возможен в текущей базе | Как ловить |
|---|---|---|
| Двойная мораль | Два массива норм и отдельный devaluation | Ledger duplicate key + legacy sentinel test |
| Двойной self-штраф | Self-поправка применялась после score | Один прогноз исхода, остаточный self_cost, фальшивый второй producer должен падать |
| Двойное дисконтирование | Project forecast уже PV, atom — будущий payoff | Известный Δ и точное ожидаемое raw U |
| Недоступные альтернативы | desired_tier и protected_cash фильтруют до Score | Экспорт generated/pruned candidates, парный тест изменения фильтра |
| Frequency→approval | Частые события легко принять за правильность | Участники часто нарушают и явно осуждают; слои должны расходиться |
| Молчание→approval | Отсутствует реакция/институт | Clock advance не меняет posterior без пригодного исхода |
| Репост→доказательство | Root перепутан с delivery | Один known root 100 раз даёт одну дозу |
| Телепатия корней | Аудит знает истинное происхождение | Одинаковая личная информация, разный скрытый source graph |
| Вытеснение физиологии | Новый нормативный фокус срезает remedies | Старый голод/сон regression и новый Norm-фокус |
| Вечная тема | Каждый чтение/кадр поднимает вопрос снова | source/revision invariant; shadow decision rate; inbox remains≠wake again |
| Самообучение от прогноза | Новые NormPrediction попадают в experience | forecast hash invariant и producer boundary tests |
| Мораль в Executor | Старое обращение a.mind.norms в complete | Отдельная собственная интерпретация; physical result одинаков при разных w |
| Кэш меняет характер | eviction удаляет редкую норму | Hot/cold round-trip и одинаковый будущий trace |
| Новая revision после решения | Данные меняются во время операции | stale input abort; сравнение source snapshot |
| Попытка получить одобрение бесконечно | Бонус за «намерение соответствовать» | Награда только за ожидаемый конечный исход; успешный план закрывается, повторный прогноз ничего не начисляет |
| Все начинают одинаково | Новая генерация читает global policy напрямую | Разные известные эпизоды; unknown group; false belief scenario |
| Норма путает стороны | Отказ B трактуется как нарушение A/наблюдателя | Три участника, собственные роли и разные Self roots |
| Время и счётчики лгут | Вторичное действие прибавляется к суткам | Duration union и раздельные overlapped intervals |
| Приёмка только постановочная | Сцена заставила NPC купить/осудить | Стартовать цель/историю, но запретить прямую команду нужного действия в автономной части |
| «Новый патч» без source | Прошлые упаковки были неполны | CRC, manifest, clean rebuild, applied patch equality |

## 7. Definition of Done

Релиз готов только если:

1. G1–G9 имеют исполняемые подтверждения, а не только текст.
2. Пройдены матрица NM и старая регрессия; новый психологический результат не куплен удалением старого теста.
3. Не менее четырёх полноценных причинных сценариев доходят до World-результата, минимум один — альтернативное решение без физического нарушения.
4. Допущена и объяснена ситуация «видит большинство, но не подражает».
5. Одобрение группы, собственный принцип и согласие партнёра не подменяют друг друга.
6. Стоимость/время/опыт, доход, одежда и сообщения сохраняют прежние контракты.
7. Restore, workers, logging, bounded attention проверены.
8. Свободные и стресс-миры раздельны, все неудачи имеют код/причину; никакой обязательной квоты дорогих покупок.
9. Исходники поставки действительно существуют и повторно собираются.

## 8. Что уже сделано именно при подготовке этого плана

Прочитаны приложенные спецификации и исходники RECOVERY 0.13, проверены актуальные README/navigation/CMake и фрагмент Planner в GitHub. Составлены новая проектная спецификация, API, этапы, тестовая матрица. Чистые формулы проверяются отдельным Python-reference с собственным отчётом.

**Не сделано:** C++ NormMemory, новые социальные действия, новый релиз или push; C++-тесты и недельные миры этого будущего патча не запускались. Python-reference не доказывает их выполнение.
