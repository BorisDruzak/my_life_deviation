# SELF-0.1 — реализация, соответствие и проверяемые границы

## База и область результата

Исполняемая версия **0.12.0-self01**, архивная база **C++ 0.11.0 experimental1**.
Это не продолжение отсутствующей поставки SELF12. Реорганизованный GitHub main
не использован как база патча и не изменён. Приложенная SELF-MODEL-0.1 сохранена
без изменений в `SELF-MODEL-0.1.md`. Статус исходного документа — проект.

Выполнен исполняемый контур SELF-0.1: результат → собственная проекция → две
когнитивные операции → атрибуция → Mind::SelfModel → PersonalView → оценка
варианта/Appraisal → Affect → Body → Capability. Под «полностью» здесь понимается
этот контур и его обязательные приёмки, не реалистичная психология человека,
не вся ранее обсуждавшаяся игра и не все возможные способы интерпретации.

Архивная структура использует `include/life`, `src/life`, `tests`; это явное
соответствие указанным в проекте путям `engine/include/life`, `engine/src/life`,
`engine/tests/life`, а не второй параллельный движок.

## Покрытие всех разделов исходной спецификации

| Разделы SELF-MODEL-0.1 | Исполняемое покрытие / проверка |
|---|---|
| 1–2: опыт вместо прямых черт; разделение Body/Cognitive/Mind | SelfModel только в Mind. Executor публикует сигнал. Никаких новых causal courage/decisiveness/shyness/confidence в Actor. `executor_cannot_directly_change_self` |
| 3–9: типы, 8 доменов, 5 осей, Estimate, SelfPrediction | `self_model.hpp/.cpp`; нейтральный prior 0.5, confidence 0; `neutral_without_experience`, `self_model_roundtrip_and_bounded_provenance` |
| 10–13: Attribution, InterpretedEpisode, реальные источники | Нормированная причинная гипотеза из собственной проекции. Forecast/hidden/replay отбрасываются. `same_visible_result_different_attributions`, `forecast_and_unperceived_episode_never_update_self` |
| 14–17: OutcomeSignal, inbox, два шага мышления | `experience.hpp/.cpp`, `cognitive_self_world.cpp`; inbox 32; InterpretOutcome и AttributeOutcome расходуют общий бюджет. `outcome_uses_two_paid_cognitive_operations`, `gate_closed_cannot_update_self` |
| 18–22: complete/fail/social → только публикация, внимание | `executor.cpp`, `social_world.cpp`, `parallel_world.cpp`; результат имеет отдельный root, не старый ID начала действия. `published_result_root_is_not_old_action_start`, `inbox_is_bounded_and_prioritizes_important_evidence` |
| 23–29: личная оценка результата, домен, атрибуция и ошибка | `interpret_outcome`, `attribute_outcome`, `context_for`; явное препятствие, действия другого, обратная связь и старые убеждения, а не world.real_reason. `external_obstacle_not_hidden_true_cause_drives_attribution`, `same_visible_result_different_attributions` |
| 30–31: Efficacy/Control | Собственная причинность взвешивает Efficacy; Control не копирует success. `self_attributed_success_increases_efficacy`, `luck_attributed_success_barely_changes_efficacy`, `known_control_can_survive_failure` |
| 32–35: отложенные Coping/UncertaintyTolerance | 8 RecoveryTrace + 8 закреплённых источников; минимум времени, собственное последующее функционирование и эмоциональный исход; та же интерпретация. `recovery_requires_later_observed_functioning_and_restores_mid_recovery`, `tolerance_requires_remembered_uncertainty` |
| 36: слабая генерализация | 20% качества наблюдения в General; 10% слабого prior для малоизученного домена. `domain_specificity_and_weak_generalization` |
| 37–39: только SelfPrediction, компоненты оценки | `Mind::view`, `Planner::forecast`, `apply_self_forecast`; контекстные затраты вместо плоского бонуса и поля решительности. `forecast_never_updates_self_or_history`, `self_behavior_scenario` |
| 40–41: Appraisal → Affect → Body → Capability | Прогноз создаёт заменяемую эмоциональную причину по домену; не обучает SelfModel. `prospective_appraisal_uses_self_during_real_forecast`, `appraisal_links_self_to_body_without_writing_biology` |
| 42–45: Mind, esteem, rejection_bias, rejection_sensitivity | SelfModel в Mind; esteem оставлено текущим аффективным состоянием; rejection_bias — совместимый профиль интерпретации, rejection_sensitivity — болезненность отказа. Перенос/переименование совместимых полей исходник прямо допускает отложить. Нет семантической подмены Acceptance/Coping этими полями |
| 46–47: биография | Только участвовавшие в исходном HistoryEvent персонажи; OutcomeSignal → тот же interpret/attribute/integrate. Нет random Self axes. `history_initializes_beliefs_from_evidence_not_random_traits` |
| 48–50: provenance, thought trace, быстрый прогноз без перебора | SelfUpdateTrace, source/episode/action, поля before/observation/after, 8 последних roots на ось. `--self-updates`, `--thoughts`. Планировщик не читает биографию; `workers_and_diagnostics_cannot_change_result` |
| 51: наблюдаемые описатели | Новые причинные «смелость/решительность» не вводились. Вместо необязательных UI-ярлыков отчёт считает инициативы/сохранение действия/пустые сравнения. Эти диагностические счётчики не участвуют в решении |
| 52–53: становление и последующее изменение | Сравнение одной и той же возможности после негативного опыта, затем self/luck successes; настоящий Planner. `self_behavior_scenario` и тесты восстановления |
| 54: SELF-I01…I09 | Все девять обязательных инвариантов покрыты тестами ниже |
| 55: 16 обязательных тестов | Таблица ниже; плюс проверки вращения provenance, источника Recovery, оплаченного мышления, бессодержательного сна и повторной доставки |
| 56–57: изменение выбора и контроль «успех ≠ развитие» | 100 одинаковых вариантов в трёх историях; источник `tools/self_behavior_scenario.cpp`, JSON результата. Реальный Planner, не простая формула желаемого счётчика |
| 58: ограниченная стоимость | 40 убеждений, фиксированные source caches/recovery/inbox. O(1) доменный snapshot; не перебор истории. Производительность всей симуляции на 1000 NPC не заявляется |
| 59: не вводить диагнозы/ярлыки | Не добавлены |
| 60: S1…S10 | Структуры, сигналы, мышление, интеграция, recovery, view, appraisal, planner, биография и разделение совместимых полей реализованы. См. `docs/plans/SELF01_IMPLEMENTATION.md` |
| 61: CMake | life_core, life_tests + CTest self_model/self_runtime/self_behavior/adaptive/career/career_runtime |
| 62: serialization | Mind/self, очередь, оба активных мыслительных шага, decision experience, recovery и job questions включены в fields(); новый save magic, прежний явно несовместим |
| 63: validation | Finite/range/domain/axis/source/sum Attribution, очереди/дубли/текущая интерпретация; World::validate вызывает проверки каждого NPC |
| 64: A/B одинаковых событий с разной интерпретацией | Модульные tests + actual Planner paired opportunities; разрешена ошибочная модель себя |
| 65–66: итоговая архитектура | Единственное runtime `self.integrate` — после AttributeOutcome; отдельный bootstrap использует ту же функцию. Иное место записи из Executor не введено |

## 16 обязательных приёмок

| № | Проверка в исходной спецификации | Реальный тест |
|---|---|---|
|1|Нейтральная модель|neutral_without_experience|
|2|Собственный успех повышает Efficacy|self_attributed_success_increases_efficacy|
|3|Удача слабо меняет Efficacy|luck_attributed_success_barely_changes_efficacy|
|4|Внешняя неудача не разрушает Efficacy|external_failure_does_not_destroy_efficacy|
|5|Неудача + восстановление → Coping|failure_can_increase_coping_after_observed_recovery|
|6|Нет ложного Coping до восстановления|failure_without_recovery_does_not_fake_coping; early_or_unobserved_recovery_is_rejected|
|7|Прогноз не меняет Self|forecast_never_updates_self_or_history|
|8|Executor не меняет Self напрямую|executor_cannot_directly_change_self|
|9|Скрытый результат не личный опыт|hidden_outcome_is_not_self_knowledge|
|10|Дубли не подкрепляются|duplicate_source_not_reinforced_even_with_new_episode_id; redelivery_of_one_social_result_is_one_self_source|
|11|Контекстность|domain_specificity_and_weak_generalization|
|12|Слабая General|domain_specificity_and_weak_generalization|
|13|Разное объяснение одинакового исхода|same_visible_result_different_attributions|
|14|Разное восстановление|same_failure_different_recovery|
|15|Save/restore внутри мышления|save_restore_mid_interpretation_and_attribution; recovery_requires_later_observed_functioning_and_restores_mid_recovery|
|16|Workers 1/2/4|workers_and_diagnostics_cannot_change_result|

## Конкретизация мест, где исходник оставляет function(...)

Это **реализованный проверочный профиль**, не выдаваемые за исходные требования
или физиологические константы коэффициенты. Источник сохранён отдельно.

* Начальные beliefs: prior=0.5, Estimate mean=0.5/variance=0.25/count=0.
* Efficacy: качество = perception_quality × importance × attribution.self;
  intentional=false не обучает собственной успешности.
* Control: качество = perception_quality × importance × attribution.confidence;
  значение = clamp(0.15+0.6*self+0.2*clarity+0.05*prior_control), независимо от success.
* Атрибуция: own=(intentional ? 0.08+0.9*feedback*clarity : 0)+0.7*self_blame;
  other=1.2*observed_other*clarity; environment=3*observed_obstacle*clarity;
  chance=0.04+uncertainty*(1-clarity)+0.08*(1-clarity); затем нормировка.
* self_blame=(1-success)*rejection_bias*(1-own_efficacy)*(1-clarity).
* Recovery минимум через 1 800 000 мс, окно истекает через 7 суток. Нужна
  последующая реально воспринятая деятельность. Завершившийся сон без восстановления
  не считается успешным функционированием.
* Coping target = clamp((0.65*emotional_recovery+0.35*(1-consequences))*(0.4+0.6*functioning)).
  Consequences — изменение собственного ощущения боли; не скрытые повреждения мира.
* Неопределённость — сохранённый перед действием прогноз. При неблагоприятном исходе
  оценка переносимости отложена до Recovery; при благоприятном может обучаться сразу.
* Self-психологическая стоимость для физических Eat/Drink/Sleep/Rest/AcquireFood/Shelter
  имеет exposure=0: модель себя не меняет калорийность еды и физическую возможность сна.
  Для личностно значимых попыток учитываются failure_exposure, uncertainty_cost,
  helplessness_cost; специфический опыт уменьшает вес общего самоубеждения.
* Subjective Acceptance задаёт слабое ожидание, но не ответ другого NPC.
  Окончательное согласие проверяет существующий исполнитель общения.

## Ограниченная память и повтор источника

8 recent_roots у SelfBelief — диагностический cache, не единственная защита.
SelfModel имеет 128 точных горячих roots и conservative retired floor отдельно
для первичных эпизодов и Recovery. Повтор старого root ниже floor отвергается
даже после вращения cache. Слишком поздний ещё не обработанный root ниже floor
также отвергается: это **явная политика допуска**, а не заявление о бесконечной
точной памяти. Поэтому результат действия получает новый root при окончании,
а source_action отдельно связывает его с давним началом. Recovery сохраняет
оригинальный root в 8 закреплённых дескрипторах; подмена action/domain запрещена.

Очередь результатов максимум 32, сохранение важных/новых и закрепление активного.
При переполнении вытеснение отражается в `dropped`. Во время перегрузки не каждый
исход обязан стать убеждением — это следствие общего ограниченного внимания.

## Область достоверности

Все формулы относятся к игровому ядру. Проверки не доказывают научную модель
психики, достоверный клинический портрет или переносимость на UE5/Windows.
100 возможностей — контролируемый ряд реальных вызовов Planner; время в нём
`charged_operation_seconds` является стоимостью когнитивных операций по модели,
не измеренной длительностью свободного принятия решения. Нет заявления, что
успешный опыт обязан ускорить каждое решение: алгоритм выполняет тот же бюджет.

Новый C++ модуль составлен из этой спецификации, а не восстановлен из отсутствующего
SELF12. Полные одежда/телефон/экономика производства/синтез произвольных процедур
не объявляются добавленными данным выпуском. Для найма реализованы конечные
конкретные вакансии, вопросы и интервью; общий рынок труда и увольнение — не цель SELF-0.1.
