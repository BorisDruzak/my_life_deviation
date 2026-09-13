# SELF-MODEL-0.1  
## Формирование модели себя через пережитый и интерпретированный опыт

**Статус:** проект спецификации  
**Целевая версия:** LIFE-0.11 / SELF-0.1  
**Назначение:** добавить в симуляцию долговременное изменение поведения NPC через формирование субъективных убеждений о самом себе.

---

# 1. Основная идея

Персонаж **не получает непосредственных числовых изменений характера от действий**.

Запрещённая схема:

```cpp
if (action.success)
    actor.confidence += 0.05;

if (action.failed)
    actor.decisiveness -= 0.03;
```

Также не вводятся причинные характеристики:

```cpp
actor.courage;
actor.decisiveness;
actor.shyness;
actor.confidence;
```

как непосредственные причины поведения.

Вместо этого:

```text
ДЕЙСТВИЕ
   ↓
ОБЪЕКТИВНЫЙ РЕЗУЛЬТАТ
   ↓
ВОСПРИЯТИЕ РЕЗУЛЬТАТА
   ↓
ИНТЕРПРЕТАЦИЯ
   ↓
АТРИБУЦИЯ ПРИЧИНЫ
   ↓
«ЧТО ЭТО ГОВОРИТ ОБО МНЕ?»
   ↓
ИЗМЕНЕНИЕ SELF MODEL
   ↓
ДРУГАЯ ОЦЕНКА БУДУЩЕЙ СИТУАЦИИ
   ↓
ДРУГОЕ РЕШЕНИЕ
```

Таким образом персонаж становится более или менее решительным **не потому, что изменилось поле `decisiveness`**, а потому что изменились его ожидания:

- получится ли у меня;
- зависит ли результат от моих действий;
- справлюсь ли я с последствиями;
- насколько опасна ошибка;
- насколько переносима неопределённость;
- насколько вероятно социальное принятие.

---

# 2. Архитектурное разделение

Целевая модель персонажа:

```text
NPC
│
├── Biology
│     физиологическая конституция
│
├── Body
│     текущее физическое состояние
│
├── Cognitive
│     базовые когнитивные способности
│
├── Affect
│     текущие эмоции
│
├── CognitiveState
│     текущий процесс мышления
│
└── Mind
      │
      ├── Learning
      ├── Knowledge
      ├── SocialMemory
      ├── Relations
      └── SelfModel       ← НОВОЕ
```

## 2.1. Что не относится к телу

Тело не знает:

```text
«я нерешительный»
«я слабый»
«у меня не получится»
«люди меня отвергают»
```

`Body` знает только объективное состояние:

```text
энергия
сон
усталость
боль
активация
повреждения
температура
кислород
...
```

## 2.2. Что является стабильной основой

Существующий `Cognitive` используется как относительно стабильная нейрокогнитивная основа:

```cpp
Cognitive {
    base[];
    learnability;
    plasticity;
}
```

То есть:

```text
Body        = что происходит с организмом сейчас.
Cognitive   = насколько этот мозг в принципе способен выполнять операции.
SelfModel   = чему этот человек научился о самом себе.
```

Это три разные сущности.

---

# 3. Новая сущность SelfModel

Создать:

```text
engine/include/life/self_model.hpp
engine/src/life/self_model.cpp
```

---

# 4. Контекстность личности

Нельзя хранить одно:

```cpp
self_efficacy = 0.63;
```

Человек может быть:

```text
очень уверенным в работе
средним в конфликте
крайне неуверенным в романтической инициативе
```

Поэтому SelfModel разделяется на домены.

```cpp
enum class SelfDomain : std::uint8_t {
    General,
    Social,
    Romance,
    Conflict,
    Work,
    Study,
    Risk,
    Recovery,
    Count
};

constexpr std::size_t self_domain_count =
    std::size_t(SelfDomain::Count);
```

`General` — слабое обобщение опыта между областями.

Остальные домены — контекстные.

---

# 5. Какие убеждения о себе храним

Необходимо минимальное количество фундаментальных осей.

```cpp
enum class SelfAxis : std::uint8_t {
    Efficacy,
    Control,
    Coping,
    Acceptance,
    UncertaintyTolerance,
    Count
};

constexpr std::size_t self_axis_count =
    std::size_t(SelfAxis::Count);
```

## 5.1. Efficacy — самоэффективность

Внутренний вопрос:

> «Если я попробую это сделать — насколько вероятно, что у меня получится?»

Это не объективная способность.

NPC может:

```text
быть способным
+
считать себя неспособным.
```

---

# 5.2. Control — ощущение контроля

Вопрос:

> «Насколько результат зависит от моих действий?»

Пример:

```text
Я хорошо подготовился
→ сдал экзамен

Control ↑
```

Другой опыт:

```text
Что бы я ни делал
→ результат случайный

Control ↓
```

---

# 5.3. Coping — способность справиться

Вопрос:

> «Если произойдёт плохой исход, смогу ли я пережить последствия?»
```

Крайне важная характеристика.

Она позволяет персонажу становиться решительнее даже после неудачи.

```text
решился
→ получил отказ
→ пережил
→ через некоторое время восстановился

Coping ↑
```

То есть:

```text
ExpectationOfSuccess может ↓
НО
Coping может ↑
```

И следующая инициатива всё равно становится легче.

---

# 5.4. Acceptance

Вопрос:

> «Насколько меня обычно принимают другие?»

Используется в социальных областях.

Не заменяет:

```cpp
SocialMemory::expected_acceptance
```

Потому что существующее значение относится к:

```text
конкретному действию
конкретному партнёру
конкретному контексту
```

SelfModel хранит более обобщённую схему:

```text
«со мной вообще обычно хотят разговаривать»
```

---

# 5.5. UncertaintyTolerance

Вопрос:

> «Могу ли я действовать, когда не знаю, чем всё закончится?»

Формируется не простым успехом.

Особенно важны эпизоды:

```text
не знал результата
→ всё равно действовал
→ катастрофы не произошло
```

или:

```text
не знал результата
→ произошла проблема
→ сумел с ней справиться
```

---

# 6. Представление одного убеждения

Используем уже существующую математическую структуру `Estimate`, но не трактуем число как «характеристику личности».

```cpp
struct SelfBelief {
    Estimate learned;

    double prior = 0.5;

    std::array<std::uint64_t, 8> recent_roots{};
    std::uint8_t root_count = 0;

    double value() const;
    double confidence() const;

    void observe(
        double value,
        double learning,
        double quality,
        double dose,
        double memory,
        std::uint64_t source
    );
};
```

При отсутствии жизненного опыта:

```text
confidence = 0

value ≈ prior
```

По мере накопления опыта:

```text
prior → всё меньше влияет
experience → всё больше влияет
```

Условно:

```cpp
double SelfBelief::value() const {
    const double c = learned.confidence();

    return (1.0 - c) * prior
         + c * unit(learned.mean);
}
```

Важно:

```text
0.73
```

означает не:

> «у персонажа уверенность 73%»

а:

> «субъективная оценка NPC утверждения  
> "в этом классе ситуаций я способен добиться результата"  
> имеет значение около 0.73».

---

# 7. SelfSchema

```cpp
struct SelfSchema {
    SelfDomain domain = SelfDomain::General;

    std::array<SelfBelief, self_axis_count> beliefs;

    Tick updated = 0;

    template<class A>
    void fields(A& a) {
        a(domain, beliefs, updated);
    }
};
```

---

# 8. SelfModel

```cpp
struct SelfModel {
    std::array<SelfSchema, self_domain_count> schemas;

    SelfPrediction predict(SelfDomain domain) const;

    bool integrate(
        const InterpretedEpisode& episode,
        const Cognitive& cognitive,
        double memory
    );

    template<class A>
    void fields(A& a) {
        a(schemas);
    }
};
```

---

# 9. SelfPrediction

Planner никогда не получает всю историю SelfModel.

Он получает только вычисленное субъективное представление.

```cpp
struct SelfPrediction {
    double efficacy = 0.5;
    double control = 0.5;
    double coping = 0.5;
    double acceptance = 0.5;
    double uncertainty_tolerance = 0.5;

    double confidence = 0.0;
};
```

Это соответствует существующей архитектуре:

```text
Mind
↓
PersonalView
↓
Planner
```

---

# 10. Самое важное: Attribution

Один и тот же результат должен менять разных персонажей по-разному.

Добавить:

```cpp
struct Attribution {
    double self = 0.0;
    double other = 0.0;
    double environment = 0.0;
    double chance = 0.0;

    double controllability = 0.5;
    double confidence = 0.0;
};
```

Условие:

```cpp
self + other + environment + chance ≈ 1.0
```

---

# 11. Пример атрибуции

Событие:

```text
NPC пригласил другого NPC встретиться.
Тот согласился.
```

### NPC A

Интерпретация:

```text
«Я правильно поговорил с ним».
```

```cpp
Attribution {
    self = 0.80,
    other = 0.10,
    environment = 0.05,
    chance = 0.05,
    controllability = 0.80
};
```

Результат:

```text
Social.Efficacy ↑
Social.Control ↑
```

### NPC B

Интерпретация:

```text
«Просто повезло».
```

```cpp
Attribution {
    self = 0.10,
    other = 0.15,
    environment = 0.05,
    chance = 0.70,
    controllability = 0.20
};
```

Тот же объективный успех почти не улучшает SelfModel.

---

# 12. Новый объект InterpretedEpisode

```cpp
struct InterpretedEpisode {
    std::uint64_t id = 0;
    std::uint64_t source_event = 0;
    std::uint64_t source_action = 0;

    SelfDomain domain = SelfDomain::General;

    Method method = Method::Idle;

    Tick time = 0;

    double success = 0.5;
    double importance = 0.0;
    double adversity = 0.0;
    double uncertainty = 0.0;

    double perception_quality = 0.0;

    Attribution attribution;

    bool intentional = false;
    bool actual = false;

    Id other = 0;
};
```

`actual` означает:

```text
это реально пережитый эпизод,
а не прогноз.
```

Это обязательный инвариант.

---

# 13. Жёсткое правило

```cpp
SelfModel::integrate()
```

обязан отклонять:

```text
forecast
imagined outcome
hypothetical branch
unperceived event
duplicate event
```

То есть прогноз:

```text
«если я подойду, мне могут отказать»
```

не может автоматически сформировать убеждение:

```text
«меня все отвергают».
```

Для изменения SelfModel нужен пережитый источник.

---

# 14. Новый cross-boundary тип OutcomeSignal

`Executor` не должен строить психологическую интерпретацию.

Он должен сообщить только результат.

Добавить, например, в:

```text
engine/include/life/cognition.hpp
```

или отдельный:

```text
engine/include/life/experience.hpp
```

Тип:

```cpp
struct OutcomeSignal {
    std::uint64_t id = 0;
    std::uint64_t action = 0;

    Method method = Method::Idle;

    Tick at = 0;

    Outcomes observed{};

    std::uint16_t observed_mask = 0;

    Id other = 0;

    bool completed = false;
    bool blocked = false;
    bool intentional = true;

    double directness = 1.0;
};
```

Этот объект ещё **ничего не говорит о личности**.

---

# 15. Изменение CognitiveState

В:

```text
engine/include/life/cognition.hpp
```

добавить:

```cpp
struct CognitiveState {
    ...

    std::vector<OutcomeSignal> outcome_inbox;

    ...
};
```

Очередь должна быть ограниченной.

Предлагаемый максимум:

```cpp
constexpr std::size_t max_outcome_inbox = 32;
```

Если событий больше:

1. сохранять критические;
2. сохранять новые;
3. низкозначимые однотипные события допускается агрегировать;
4. запрещается бесконечное накопление.

---

# 16. Новые Cognitive Operation

Расширить:

```cpp
enum class Operation {
    None,
    Interpret,
    Recall,
    Forecast,
    Compare,
    Commit,
    Recognize,
    Reply,
    SocialReply,
    SocialObserve,

    InterpretOutcome,   // новое
    AttributeOutcome   // новое
};
```

Почему две операции:

```text
InterpretOutcome:
«Что вообще произошло?»

AttributeOutcome:
«Почему это произошло и что это говорит обо мне?»
```

Это позволяет когнитивным способностям реально ограничивать глубину самоанализа.

---

# 17. Полный runtime pipeline

```text
┌──────────────────────────────┐
│          EXECUTOR            │
└──────────────┬───────────────┘
               │
         ActionResult
               │
               ▼
        OutcomeSignal
               │
               ▼
┌──────────────────────────────┐
│       COGNITIVE STATE        │
│       outcome_inbox          │
└──────────────┬───────────────┘
               │
          Attention
               │
               ▼
        InterpretOutcome
               │
               ▼
      субъективный результат
               │
               ▼
       AttributeOutcome
               │
               ▼
      InterpretedEpisode
               │
         ┌─────┴─────┐
         ▼           ▼
     Learning     SelfModel
         │           │
         └─────┬─────┘
               ▼
             Mind
               │
               ▼
        PersonalView
               │
               ▼
            Planner
```

---

# 18. Изменения executor.cpp

Файл:

```text
engine/src/life/executor.cpp
```

Сейчас именно здесь завершается действие.

После фактического результата необходимо создавать только `OutcomeSignal`.

Например:

```cpp
void World::complete(Actor& actor) {
    ...

    OutcomeSignal outcome;

    outcome.id = state_.next_id++;
    outcome.action = actor.action.id;
    outcome.method = actor.action.method;
    outcome.at = state_.now;
    outcome.completed = true;
    outcome.intentional = true;

    ...

    publish_outcome(actor, outcome);
}
```

---

# 19. Чего executor.cpp делать НЕ должен

Запрещено:

```cpp
actor.mind.self
    .schema(SelfDomain::Social)
    .efficacy += ...;
```

Запрещено:

```cpp
actor.confidence += ...;
```

Запрещено:

```cpp
actor.mind.self.integrate(...);
```

То есть зависимость должна быть:

```text
Executor
    ↓
OutcomeSignal
```

а не:

```text
Executor
    ↓
Mind
```

---

# 20. Ошибки действия тоже являются опытом

`World::fail()` должен также публиковать результат.

Например:

```cpp
OutcomeSignal outcome;

outcome.completed = false;
outcome.blocked = true;
outcome.method = actor.action.method;
outcome.action = actor.action.id;

publish_outcome(actor, outcome);
```

Но причина ошибки должна передаваться только в объёме, который NPC реально способен получить.

Например:

```text
дорога физически закрыта
+
NPC дошёл до препятствия

→ environment cause может быть известна
```

Но:

```text
магазин закрыт в другом конце деревни,
NPC туда не ходил

→ никаких изменений SelfModel.
```

---

# 21. Новая функция publish_outcome

В `World`:

```cpp
void World::publish_outcome(
    Actor& actor,
    OutcomeSignal outcome
);
```

Пример:

```cpp
void World::publish_outcome(
    Actor& actor,
    OutcomeSignal outcome
) {
    auto& inbox = actor.cog.outcome_inbox;

    if (inbox.size() >= max_outcome_inbox)
        compact_outcomes(inbox);

    inbox.push_back(std::move(outcome));

    ++actor.cog.situation_version;

    actor.cog.review_at =
        std::min(actor.cog.review_at, state_.now);
}
```

Функция ничего не интерпретирует.

---

# 22. Обработка в cognitive_world.cpp

Файл:

```text
engine/src/life/cognitive_world.cpp
```

В построение `AttentionTopic` добавить реальные исходы.

Условно:

```cpp
for (const auto& outcome : c.outcome_inbox) {

    const double priority =
        outcome_priority(outcome, actor);

    input.push_back({
        make_outcome_topic(outcome.id),
        TopicKind::Memory,
        ...,
        priority,
        ...
    });
}
```

Значимость должна зависеть от:

```text
силы результата
важности текущей цели
неожиданности
эмоциональной реакции
социальной значимости
угрозы
```

---

# 23. InterpretOutcome

Первая операция отвечает:

```text
Что произошло для меня?
```

Не:

```text
Что объективно произошло в мире?
```

Условно:

```cpp
InterpretedEpisode interpret_outcome(
    const Actor& actor,
    const OutcomeSignal& signal
);
```

Пример:

```cpp
InterpretedEpisode e;

e.id = signal.id;
e.source_event = signal.id;
e.source_action = signal.action;
e.method = signal.method;
e.time = signal.at;

e.domain = domain_of(signal.method);

e.success =
    subjective_success(actor, signal);

e.adversity =
    subjective_adversity(actor, signal);

e.importance =
    subjective_importance(actor, signal);

e.uncertainty =
    remembered_uncertainty(actor, signal.action);

e.perception_quality =
    current_perception_quality(actor);

e.intentional = signal.intentional;
e.actual = true;
```

---

# 24. Определение domain

Добавить:

```cpp
SelfDomain domain_of(Method method);
```

Например:

```cpp
switch (method) {

case Method::Talk:
case Method::Social:
    return SelfDomain::Social;

case Method::PrivateIntimacy:
    return SelfDomain::Romance;

case Method::Work:
    return SelfDomain::Work;

case Method::Study:
    return SelfDomain::Study;

case Method::Inspect:
    return SelfDomain::General;

default:
    return SelfDomain::General;
}
```

Для конкретных социальных `Interaction`:

```cpp
SelfDomain domain_of(Interaction interaction);
```

Например:

```text
RomanticTouch → Romance

AskInfo → Social

BorrowItem → Social

ClaimItem → Conflict/Social
```

---

# 25. AttributeOutcome

После определения результата NPC должен решить:

```text
почему это произошло?
```

```cpp
Attribution attribute(
    const Actor& actor,
    const InterpretedEpisode& episode,
    const OutcomeSignal& source
);
```

---

# 26. Атрибуция не должна знать скрытую истину

Запрещено:

```cpp
if (world.real_reason == ...)
```

Разрешено использовать только:

```text
воспринятые обстоятельства
известные препятствия
собственные намерения
ответ другого NPC
прошлый опыт
доступные причинные знания
```

---

# 27. Условная формула атрибуции

Это игровая математическая модель, а не утверждение о полной научной модели человека.

Сначала формируем свидетельства:

```cpp
double self_evidence;
double other_evidence;
double environment_evidence;
double chance_evidence;
```

Например:

```cpp
self_evidence =
    episode.intentional
    * action_feedback
    * causal_clarity;

other_evidence =
    observed_other_action
    * causal_clarity;

environment_evidence =
    observed_obstacle
    * causal_clarity;

chance_evidence =
    episode.uncertainty
    * (1.0 - causal_clarity);
```

Нормализуем:

```cpp
double sum =
      self_evidence
    + other_evidence
    + environment_evidence
    + chance_evidence;

if (sum > 0) {
    result.self        = self_evidence / sum;
    result.other       = other_evidence / sum;
    result.environment = environment_evidence / sum;
    result.chance      = chance_evidence / sum;
}
```

---

# 28. Причинное мышление влияет на Attribution

Важно сохранить уже принятый принцип:

```text
Ассоциативное обучение
≠
Причинное мышление
```

NPC с низкой способностью причинного анализа всё равно способен хорошо выучить:

```text
«после разговоров мне обычно неприятно»
```

Но может плохо определить:

```text
почему именно разговор закончился плохо.
```

Поэтому:

```cpp
causal_clarity =
    function(
        actor.mind.cognition,
        actor.mind.knowledge,
        perception_quality,
        available_evidence
    );
```

Чем хуже причинное мышление, тем больше:

```text
chance
старые ожидания
ошибочная генерализация
```

могут влиять на объяснение события.

---

# 29. Ошибочная модель самого себя ОБЯЗАТЕЛЬНА

Система не должна стремиться автоматически устанавливать объективную истину.

Пример:

```text
Реальная причина отказа:
другой NPC занят.
```

NPC знает лишь:

```text
мне отказали.
```

Интерпретация:

```text
«Я никому не интересен».
```

SelfModel может измениться отрицательно.

То есть:

```text
объективно неправильное убеждение
```

является допустимым состоянием симуляции.

Именно это создаёт психологически интересных NPC.

---

# 30. Обновление Efficacy

Пусть:

```text
S = субъективный успех [0..1]
Aself = вероятность собственной причинности [0..1]
Q = качество интерпретации [0..1]
```

Тогда:

```cpp
quality =
    Aself
    * episode.perception_quality
    * episode.importance;
```

И:

```cpp
efficacy.observe(
    episode.success,
    cognitive.learnability,
    quality,
    1.0,
    memory,
    episode.id
);
```

Следствие:

```text
успех + «я это сделал»
→ сильное обучение

успех + «просто повезло»
→ слабое обучение
```

---

# 31. Обновление Control

Важно не путать успех и контроль.

Пример:

```text
Я сделал действие.
Получил плохой результат.
Но понимаю, почему это произошло.
```

Тогда:

```text
Efficacy может ↓

Control не обязательно ↓.
```

Условная цель:

```cpp
double observed_control =
    unit(
        attribution.self
        * attribution.controllability
    );
```

Но отрицательный результат, возникший от понятного собственного действия, не должен автоматически трактоваться как беспомощность.

Поэтому реальная формула должна учитывать:

```text
предсказуемость результата
понимание причин
наличие альтернатив
возможность коррекции
```

---

# 32. Coping нельзя обновлять сразу

Это принципиально.

В момент отказа NPC ещё не знает:

```text
сможет ли он пережить отказ.
```

Поэтому нужен отложенный процесс.

Добавить:

```cpp
struct RecoveryTrace {
    std::uint64_t episode = 0;
    SelfDomain domain = SelfDomain::General;

    Tick started = 0;
    Tick evaluate_after = 0;

    double initial_distress = 0;

    bool active = false;
};
```

Хранить ограниченное количество:

```cpp
std::array<RecoveryTrace, 8>
```

---

# 33. Как формируется Coping

После неприятного эпизода:

```text
distress_before = высокий
```

Через определённое время:

```text
страх ↓
тревога ↓
персонаж функционирует
продолжает деятельность
не произошла ожидаемая катастрофа
```

Тогда:

```cpp
coping_result =
    function(
        emotional_recovery,
        preserved_function,
        actual_consequences
    );
```

И только после этого:

```cpp
SelfModel.Coping ↑
```

Таким образом отрицательный опыт способен сделать человека сильнее.

---

# 34. UncertaintyTolerance

Обновлять только если перед действием действительно была субъективная неопределённость.

Нужно сохранить в action/cognitive trace:

```cpp
struct DecisionExperience {
    std::uint64_t action = 0;

    double predicted_uncertainty = 0;
    double predicted_risk = 0;

    SelfPrediction self_prediction;
};
```

Это не скрытая истина.

Это то, **что NPC думал перед действием**.

После результата:

```text
высокая неопределённость
+
NPC всё равно действовал
+
результат оказался переносим

→ uncertainty_tolerance ↑
```

---

# 35. Интеграция SelfModel

Условный код:

```cpp
bool SelfModel::integrate(
    const InterpretedEpisode& e,
    const Cognitive& cognitive,
    double memory
) {
    if (!e.actual)
        return false;

    if (!e.id)
        return false;

    auto& schema =
        schemas[std::size_t(e.domain)];

    const double q =
        e.perception_quality
        * e.importance;

    const double self_q =
        q * e.attribution.self;

    schema.beliefs[
        std::size_t(SelfAxis::Efficacy)
    ].observe(
        e.success,
        cognitive.learnability,
        self_q,
        1.0,
        memory,
        e.id
    );

    schema.beliefs[
        std::size_t(SelfAxis::Control)
    ].observe(
        e.attribution.controllability,
        cognitive.learnability,
        q * e.attribution.confidence,
        1.0,
        memory,
        e.id
    );

    schema.updated = e.time;

    return true;
}
```

`Coping` и часть `UncertaintyTolerance` обновляются позже через `RecoveryTrace`.

---

# 36. Генерализация между доменами

Нельзя делать:

```text
успех на работе
→ я теперь уверен в романтических отношениях
```

Но слабая генерализация допустима.

Например:

```text
Work.Efficacy += полный вес

General.Efficacy += 15–25% веса
```

Условно:

```cpp
general_quality =
    domain_quality * 0.20;
```

И наоборот:

```text
General
```

может слабо влиять на неизвестные домены.

---

# 37. PersonalView

В:

```text
engine/include/life/mind.hpp
```

расширить `PersonalView`.

Не копировать туда память и источники.

Добавить:

```cpp
std::array<
    SelfPrediction,
    self_domain_count
> self_predictions;
```

И в:

```cpp
Mind::view(...)
```

или при построении `World::personal_view()`:

```cpp
for (std::size_t i = 0;
     i < self_domain_count;
     ++i) {

    view.self_predictions[i] =
        self.predict(SelfDomain(i));
}
```

Planner получает только итоговые субъективные ожидания.

---

# 38. Planner не получает «решительность»

Запрещено:

```cpp
score *= actor.decisiveness;
```

Использовать:

```cpp
const auto& self =
    view.self_predictions[
        std::size_t(domain_of(option.method))
    ];
```

---

# 39. Влияние SelfModel на оценку действия

Предлагается не давать плоский бонус.

Вычислять компоненты.

Например:

```cpp
double failure_exposure =
    importance
    * (1.0 - self.efficacy)
    * (1.0 - self.coping);

double uncertainty_cost =
    uncertainty
    * (1.0 - self.uncertainty_tolerance);

double helplessness_cost =
    goal_importance
    * (1.0 - self.control);
```

После чего:

```cpp
subjective_cost +=
      failure_exposure
    + uncertainty_cost
    + helplessness_cost;
```

Так нерешительность возникает естественно.

---

# 40. Связь с Appraisal

Это одна из важнейших интеграций.

Существующий:

```cpp
Appraisal.control
```

должен получать значение не из объективного мира, а из субъективной SelfPrediction.

Условно:

```cpp
appraisal.control =
    self.control;
```

А:

```cpp
appraisal.uncertainty =
    forecast_uncertainty;

appraisal.harm =
    expected_negative_consequence
    * (1.0 - self.coping);
```

Получается:

```text
низкий Control
      ↓
Fear / Anxiety ↑
      ↓
Body.activation ↑
      ↓
Capability изменяется
      ↓
решение становится сложнее
```

То есть прошлый опыт реально доходит до тела через интерпретацию.

---

# 41. Правильная обратная связь

Полная петля:

```text
ПАМЯТЬ
   ↓
SELF MODEL
   ↓
APPRAISAL
   ↓
ЭМОЦИЯ
   ↓
ТЕЛО
   ↓
CAPABILITY
   ↓
КОГНИТИВНЫЙ ПРОЦЕСС
   ↓
РЕШЕНИЕ
   ↓
ДЕЙСТВИЕ
   ↓
РЕЗУЛЬТАТ
   ↓
ВОСПРИЯТИЕ
   ↓
ИНТЕРПРЕТАЦИЯ
   ↓
SELF MODEL
```

Это правильная двусторонняя схема:

```text
Тело влияет на психику.

Психика влияет на тело.

Но они не являются одной сущностью.
```

---

# 42. Mind

Изменить:

```text
engine/include/life/mind.hpp
```

Добавить:

```cpp
#include "life/self_model.hpp"
```

И:

```cpp
struct Mind {
    SocialMemory social;
    Cognitive cognition;
    Attention attention;
    Learning learning;
    Knowledge knowledge;

    SelfModel self; // NEW

    ...
};
```

Не помещать SelfModel в:

```cpp
Body
CognitiveState
Actor root
```

Он является долговременной частью `Mind`.

---

# 43. Что делать с Actor::esteem

Текущее значение не уничтожать в первой реализации.

Но семантически определить его как:

```text
текущее чувство собственной ценности /
текущая эмоциональная оценка себя
```

А не:

```text
долговременная самооценка личности.
```

То есть:

```text
SelfModel = долговременные убеждения.

esteem = текущее состояние.
```

В дальнейшем желательно переименовать:

```cpp
esteem
```

в:

```cpp
felt_esteem
```

или аналогичное понятие.

---

# 44. Что делать с rejection_bias

Текущий:

```cpp
CognitiveState::rejection_bias
```

концептуально не является текущим когнитивным состоянием.

Если это устойчивый стиль интерпретации, позднее вынести:

```cpp
struct InterpretationProfile {
    double negative_attribution_bias;
    double self_blame_bias;
    double externalization_bias;
    double rejection_bias;
};
```

В:

```cpp
Mind
```

либо в отдельный стабильный профиль.

SELF-0.1 может пока сохранить старое поле для совместимости.

---

# 45. Что делать с SocialMemory::rejection_sensitivity

На первом этапе сохранить.

Но определить различия:

```text
rejection_sensitivity
=
насколько эмоционально болезненно воспринимается отказ

SelfModel.Acceptance
=
насколько NPC ожидает социального принятия

SelfModel.Coping
=
насколько NPC считает, что способен пережить отказ
```

Это три разных механизма.

---

# 46. История до начала игры

Очень важное правило:

нельзя генерировать NPC так:

```cpp
actor.mind.self.efficacy = random();
actor.mind.self.coping = random();
```

Иначе мы снова получаем необъяснимый набор характеристик.

Сейчас `generation.cpp` уже создаёт исторические события.

Для SelfModel необходимо использовать их.

Добавить:

```cpp
bootstrap_self_history(
    Actor& actor,
    const HistoryEvent& event,
    ...
);
```

Но эта функция должна создавать тот же:

```cpp
InterpretedEpisode
```

который использует runtime.

То есть:

```text
сгенерированное прошлое
↓
исторический InterpretedEpisode
↓
SelfModel::integrate()
```

а не прямое выставление полей.

---

# 47. Почему это принципиально

Тогда для любого NPC можно сказать:

```text
Почему Social.Efficacy = 0.31?
```

и получить:

```text
история:
#182 — негативный опыт
#291 — отказ
#420 — успех при слабой self-attribution
#713 — повторный отказ
```

Личность оказывается следствием биографии.

---

# 48. Debug provenance

Добавить диагностический вывод:

```cpp
struct SelfUpdateTrace {
    Id actor = 0;

    std::uint64_t episode = 0;

    SelfDomain domain;
    SelfAxis axis;

    double before = 0;
    double observation = 0;
    double after = 0;

    double attribution_self = 0;
    double quality = 0;

    std::uint64_t source = 0;
};
```

Использовать только как diagnostics.

Он не должен участвовать в симуляции.

---

# 49. Thought trace

Желательно расширить:

```cpp
ThoughtKind
```

значениями:

```cpp
InterpretOutcome
Attribute
SelfUpdate
```

Пример диагностической цепочки:

```text
EVENT #812

Notice:
«получен отказ»

InterpretOutcome:
«результат неблагоприятный»

Attribute:
«вероятная причина — моё действие»

SelfUpdate:
Social.Efficacy:
0.52 → 0.49
```

Это позволит отлаживать психологическую причинность.

---

# 50. NPC не обязан вспоминать источник во время решения

Обычный Planner НЕ делает:

```text
Recall event #812
Recall event #474
Recall event #124
→ решение
```

Он использует уже сформированную схему:

```text
Social.Efficacy = .37
Control = .31
Coping = .66
```

То есть:

```text
старые события
↓
когда-то сформировали убеждение
↓
убеждение используется автоматически
```

Эпизодическая память нужна только если отдельная когнитивная операция действительно вызывает воспоминание.

---

# 51. Наблюдаемые характеристики

Для UI/debug можно вычислять:

```text
решительность
смелость
уверенность
настойчивость
социальная тревожность
```

Но только как **Derived Behaviour Metrics**.

Например:

```cpp
struct BehaviourDescriptor {
    double decisiveness;
    double persistence;
    double social_confidence;
};
```

И:

```cpp
BehaviourDescriptor describe(
    const BehaviourHistory& history
);
```

Planner никогда не читает эту структуру.

Это описание поведения наблюдателем.

---

# 52. Пример рождения «нерешительного человека»

История:

```text
Событие 1:
проявил инициативу
→ негативный исход
→ self attribution высокая

Событие 2:
сам решил
→ наказание

Событие 3:
сам решил
→ ошибка
→ долго переживал

Событие 4:
решение за него принял другой
→ хороший исход
```

SelfModel постепенно получает:

```text
Efficacy ↓
Control ↓
Coping ↓
UncertaintyTolerance ↓
```

Новая ситуация:

```text
«предложить идею начальнику»
```

Planner видит:

```text
успех неизвестен
Efficacy = .28
Control = .31
Coping = .34
UncertaintyTolerance = .26
```

Appraisal:

```text
uncertainty ↑
control ↓
harm ↑
```

Далее:

```text
Anxiety ↑
Fear ↑
Body.activation ↑
```

NPC:

```text
дольше анализирует
чаще ищет альтернативы
чаще отказывается
чаще ждёт
```

Наблюдатель говорит:

> «Он нерешительный».

Но в симуляции такой характеристики нет.

---

# 53. Как тот же NPC меняется

Дальнейшая история:

```text
решился
→ получилось

решился
→ получилось

решился
→ не получилось
→ исправил последствия

получил отказ
→ пережил отказ

действовал в неопределённости
→ катастрофы не произошло
```

Изменения:

```text
Efficacy ↑
Control ↑
Coping ↑↑
UncertaintyTolerance ↑
```

Следовательно:

```text
Appraisal.control ↑
expected harm ↓
anxiety ↓
```

И тот же Planner начинает чаще выбирать действие.

Никакого:

```cpp
decisiveness += 10;
```

не произошло.

---

# 54. Обязательные инварианты

## SELF-I01

`Executor` никогда не изменяет `SelfModel`.

---

## SELF-I02

Прогноз не изменяет SelfModel.

```text
forecast != experience
```

---

## SELF-I03

Скрытое событие мира не изменяет SelfModel.

```text
unperceived world state
→ no self update
```

---

## SELF-I04

Один source event не может быть применён дважды.

---

## SELF-I05

Одинаковое объективное событие может вызвать разные SelfModel updates у разных NPC.

---

## SELF-I06

Контекстные убеждения не должны мгновенно распространяться на все области жизни.

---

## SELF-I07

Негативный исход не обязан ухудшать персонажа.

Он способен:

```text
Efficacy ↓
Coping ↑
```

одновременно.

---

## SELF-I08

Положительный исход не обязан улучшать SelfModel.

Если:

```text
attribution.self ≈ 0
```

эффект может быть минимальным.

---

## SELF-I09

Planner читает только `SelfPrediction` из `PersonalView`.

Planner не получает:

```text
World
SelfModel history
evidence roots
реальную причину события
```

---

# 55. Новые тесты

Создать:

```text
engine/tests/life/test_self_model.cpp
```

---

## TEST 1 — neutral_without_experience

```cpp
TEST("self", neutral_without_experience) {
    SelfModel m;

    auto x = m.predict(SelfDomain::Social);

    NEAR(x.efficacy, .5, ...);
    CHECK(x.confidence == 0);
}
```

---

## TEST 2 — self_attributed_success_increases_efficacy

```text
успех
+
self attribution=.9

→ efficacy растёт
```

---

## TEST 3 — luck_attributed_success_barely_changes_efficacy

Одинаковый успех:

```text
A: «я сделал»
B: «повезло»
```

После:

```text
A.efficacy > B.efficacy
```

---

## TEST 4 — external_failure_does_not_destroy_efficacy

```text
действие не удалось
+
явное внешнее препятствие

→ Efficacy почти не падает
```

---

## TEST 5 — failure_can_increase_coping

```text
неудача
→ distress
→ восстановление

Coping ↑
```

---

## TEST 6 — failure_without_recovery_does_not_fake_coping

До завершения RecoveryTrace:

```text
Coping не меняется.
```

---

## TEST 7 — forecast_never_updates_self

Создать несколько:

```text
Planner::forecast(...)
```

SelfModel hash остаётся прежним.

---

## TEST 8 — executor_cannot_directly_change_self

Очень важный интеграционный тест.

```text
autonomy = false

NPC выполняет действие
→ OutcomeSignal появился
→ SelfModel НЕ изменился

autonomy = true
→ когнитивная обработка произошла
→ только теперь SelfModel может измениться
```

---

## TEST 9 — hidden_outcome_is_not_self_knowledge

Изменить скрытое состояние другого NPC/мира.

Если оно не было воспринято:

```text
SelfModel идентичен.
```

---

## TEST 10 — duplicate_episode_not_reinforced

Повторная передача одного `episode.id`:

```text
нет второго изменения.
```

---

## TEST 11 — domain_specificity

```text
10 успешных Work episodes
```

должны сильно увеличить:

```text
Work.Efficacy
```

но не должны значительно увеличить:

```text
Romance.Efficacy
```

---

## TEST 12 — weak_generalization

При опыте Work:

```text
General.Efficacy
```

может немного измениться, но:

```text
ΔGeneral << ΔWork
```

---

## TEST 13 — same_event_different_interpretation

Два NPC получают одинаковый отказ.

У NPC A:

```text
self attribution высокая.
```

У NPC B:

```text
external attribution высокая.
```

Их SelfModel расходится.

---

## TEST 14 — same_failure_different_coping

Два NPC имеют одинаковый отказ.

Один быстро восстанавливается.

Другой долго сохраняет высокий distress.

После recovery window:

```text
A.Coping > B.Coping
```

---

## TEST 15 — save_restore_preserves_self_model

Сохранение посередине интерпретации:

```text
save
load
run
```

должно дать тот же hash.

---

## TEST 16 — worker_count_determinism

```text
1 worker
2 workers
4 workers
```

не изменяют SelfModel history/result.

---

# 56. Тест изменения поведения

Необходимо проверить главное:

не просто изменение чисел, а изменение выбора.

Сценарий:

```text
NPC имеет низкую Social.Efficacy.
```

100 одинаковых допустимых социальных возможностей.

Зафиксировать:

```text
инициативы
отказы от действия
среднее время принятия решения
```

После серии корректно интерпретированных успешных опытов:

```text
Social.Efficacy ↑
Control ↑
```

Повторить сценарий.

Требование:

```text
частота социальных инициатив должна статистически вырасти
```

при неизменных:

```text
Biology
Body initial state
external opportunity distribution
```

То есть проверяем настоящую причинную цепочку.

---

# 57. Контрольный тест: успех не равен развитию

Серия:

```text
20 объективных успехов
```

но:

```text
self attribution ≈ 0
chance ≈ 1
```

Не должна давать такое же изменение поведения, как:

```text
20 успехов
+
self attribution ≈ 1.
```

Это критический тест всей механики.

---

# 58. Performance constraints

Для 1000+ NPC запрещается:

```text
каждый decision
→ перебирать всю биографию.
```

SelfModel должен иметь фиксированный размер:

```text
8 domains
×
5 axes
≈
40 beliefs / NPC
```

Это крайне дёшево.

Решение использует:

```text
O(1)
```

доступ к нужному domain.

Источники истории ограничиваются небольшим provenance cache.

---

# 59. Что НЕ делать в SELF-0.1

Не добавлять сейчас:

```text
DSM-подобные диагнозы
«интроверт»
«экстраверт»
«трус»
«смелый»
«нарцисс»
«депрессивный»
```

как непосредственные параметры.

Также не строить пока огромную психологическую онтологию.

Нужен фундамент:

```text
Experience
→ Interpretation
→ Attribution
→ SelfModel
→ Future appraisal
```

После этого более сложные явления можно получать комбинациями.

---

# 60. Порядок реализации

## Этап S1 — структуры без изменения поведения

Создать:

```text
self_model.hpp
self_model.cpp
```

Реализовать:

```text
SelfDomain
SelfAxis
SelfBelief
SelfSchema
SelfModel
SelfPrediction
Attribution
InterpretedEpisode
```

Добавить в `Mind`.

Тесты unit.

Поведение симуляции пока не менять.

---

## Этап S2 — OutcomeSignal

Добавить:

```text
OutcomeSignal
outcome_inbox
publish_outcome()
```

Подключить:

```text
complete()
fail()
social result
```

Проверить:

```text
Action → signal
```

но SelfModel пока не обновлять.

---

## Этап S3 — когнитивная интерпретация

Добавить:

```text
InterpretOutcome
AttributeOutcome
```

в `CognitiveState`.

Сформировать:

```text
OutcomeSignal
→ InterpretedEpisode
```

Добавить debug trace.

---

## Этап S4 — SelfModel integration

После законченной Attribution:

```cpp
actor.mind.self.integrate(...);
```

Это должно быть **единственное runtime-место**, откуда изменяется SelfModel.

---

## Этап S5 — Recovery / Coping

Реализовать:

```text
RecoveryTrace
```

и отложенное обучение coping.

---

## Этап S6 — PersonalView

Передавать:

```cpp
SelfPrediction
```

в Planner.

На этом этапе поведение пока можно оставить прежним.

---

## Этап S7 — Appraisal

Связать:

```text
SelfModel.Control
SelfModel.Coping
```

с:

```text
Appraisal.control
Appraisal.harm
Appraisal.uncertainty
```

Получить:

```text
SelfModel
→ emotion
→ body
```

---

## Этап S8 — Planner

Подключить:

```text
Efficacy
Control
Coping
UncertaintyTolerance
```

к субъективной стоимости вариантов.

На этом этапе должно появиться наблюдаемое изменение поведения.

---

## Этап S9 — историческое формирование персонажей

`generation.cpp` перестаёт задавать новые личностные свойства случайными числами.

Сгенерированная история:

```text
HistoryEvent
→ historical InterpretedEpisode
→ SelfModel::integrate()
```

После генерации каждый NPC имеет объяснимую личную историю.

---

## Этап S10 — убрать прямые псевдо-черты

После стабилизации определить судьбу:

```text
esteem
rejection_bias
rejection_sensitivity
прочих прямых коэффициентов
```

Разделить их на:

```text
стабильный темперамент
текущее состояние
выученное убеждение
```

и убрать семантическое дублирование.

---

# 61. Изменения CMake

Добавить:

```cmake
engine/src/life/self_model.cpp
```

в:

```cmake
life_core
```

И:

```cmake
engine/tests/life/test_self_model.cpp
```

в:

```cmake
life_tests
```

Добавить:

```cmake
add_test(NAME self_model COMMAND life_tests self_model)
```

---

# 62. Serialization

Поскольку `SelfModel` становится частью `Mind`, он должен сериализоваться обычным `fields()`.

State format необходимо изменить, например:

```text
LIFE-0.10.0
→
LIFE-0.11.0
```

Старые save-файлы либо:

1. явно считаются несовместимыми;
2. либо мигрируются с нейтральным SelfModel.

Для research core предпочтительнее явная несовместимость, чем скрытая неправильная миграция.

---

# 63. Validation

Добавить проверки:

```text
все значения finite
prior ∈ [0,1]
attribution components ∈ [0,1]
sum attribution ≈ 1
source != 0 для learned evidence
domain < Count
axis < Count
нет duplicate source
```

`World::validate()` должен проверять SelfModel каждого активного NPC.

---

# 64. Главный acceptance scenario

Создать эталонный тестовый NPC.

Начальное состояние:

```text
Social.Efficacy ≈ .50
Social.Control ≈ .50
Social.Coping ≈ .50
```

## История A

```text
инициатива → успех → self attribution
инициатива → успех → self attribution
инициатива → отказ → быстро восстановился
инициатива → успех → self attribution
```

Ожидается:

```text
Efficacy ↑
Control ↑
Coping ↑
```

И рост инициативности.

## История B

Те же объективные события.

Но интерпретация:

```text
успех → случайность
успех → другой человек
отказ → моя вина
успех → случайность
```

Ожидается:

```text
существенно более низкая Efficacy
более низкий Control
другая будущая стратегия
```

То есть два NPC прожили почти одинаковую объективную жизнь, но получили разный характер.

Это один из главных критериев успеха SELF-0.1.

---

# 65. Итоговая архитектура

```text
                ОБЪЕКТИВНЫЙ МИР
                       │
                       ▼
                   Outcome
                       │
                       ▼
                  Perception
                       │
                       ▼
               OutcomeSignal
                       │
                       ▼
              Cognitive Process
                       │
            ┌──────────┴──────────┐
            ▼                     ▼
       Interpretation         Attribution
                                  │
                                  ▼
                          InterpretedEpisode
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
              World Learning                SelfModel
        «что обычно бывает»       «что это говорит обо мне»
                    │                           │
                    └─────────────┬─────────────┘
                                  ▼
                                Mind
                                  │
                                  ▼
                            PersonalView
                                  │
                                  ▼
                               Planner
                                  │
                                  ▼
                              Appraisal
                                  │
                                  ▼
                               Affect
                                  │
                                  ▼
                                Body
                                  │
                                  ▼
                             Capability
                                  │
                                  ▼
                         следующее решение
```

---

# 66. Фундаментальный принцип SELF-0.1

**Персонаж не меняет характер потому, что произошло событие.**

Персонаж меняется потому, что:

```text
он пережил событие,
воспринял его,
интерпретировал его,
приписал ему причины,
встроил вывод в представление о себе,
а новое представление изменило следующие оценки и решения.
```

Поэтому:

```text
действие ≠ изменение характеристики
```

Правильная зависимость:

```text
действие
→ опыт
→ убеждение
→ поведение
```

Именно `SelfModel`, а не набор `courage/confidence/decisiveness`, должен стать фундаментом приобретённой личности NPC.