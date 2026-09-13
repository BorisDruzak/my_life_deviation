# NORM-MEMORY 0.1: покрытие матрицы приёмки

Аудит сопоставляет каждый сценарий `NM-001`…`NM-087` из
`NORM_MEMORY_0_1_TEST_MATRIX.json` с фактической функцией `TEST(...)`. Имя с
номером само по себе не считается доказательством: в колонке «Проверяемый
смысл» указано наблюдаемое утверждение теста. Статус «частично» означает, что
существенная часть инварианта проверена, но один из заявленных в матрице
аспектов остаётся без прямой проверки.

На срезе r4 профильный CTest подтвердил 154 теста без ошибок
в восьми файлах `test_norm_*.cpp`: 34 memory, 13 ledger, 7 social, 4 phone,
23 runtime, 10 persistence, 45 planner и 18 conformance. Полный запуск всей
регрессии r4: 599 cases, 0 failures. Карта ниже также
ссылается на пять ранее существовавших тестов в `test_social.cpp`,
`test_adaptive.cpp`, `test_civil_unit.cpp` и `test_phone.cpp`.

| ID | Функция и файл | Проверяемый смысл | Статус |
|---|---|---|---|
| NM-001 | `nm001_no_evidence_is_unknown_not_social_permission` — `engine/tests/life/test_norm_memory.cpp` | Пустая память даёт неизвестность, техническое 0.5 и нулевое покрытие без разрешения действия. | полное |
| NM-002 | `nm002_descriptive_evidence_does_not_change_approval_or_principle` — `test_norm_memory.cpp` | Описательное свидетельство не меняет Approval и личный принцип. | полное |
| NM-003 | `nm003_approval_keeps_indifference_as_a_third_outcome` — `test_norm_memory.cpp` | Проверены три независимые категории Approval и численный posterior 6/11, 3/11, 2/11. | полное |
| NM-004 | `nm004_invalid_input_throws_before_any_mutation` — `test_norm_memory.cpp` | Некорректный ввод отвергается до изменения сериализованного состояния. | полное |
| NM-005 | `nm005_boundary_value_types_round_trip_without_world_state` — `test_norm_memory.cpp` | Публичные value-типы сериализуются без `World`/`world_root` и чужого мнения. | полное |
| NM-006 | `nm006_eight_of_ten_has_finite_uncertain_prediction` — `test_norm_memory.cpp` | Проверены p=.75 и coverage=10/14. | полное |
| NM-007 | `nm007_quality_is_multiplied_once` — `test_norm_memory.cpp` | Quality/confidence/reliability/dose дают вес .168 один раз. | полное |
| NM-008 | `nm008_duplicate_delivery_has_one_contribution_and_revision` — `test_norm_memory.cpp` | Повтор delivery даёт один вклад и одну semantic revision. | полное |
| NM-009 | `nm009_known_root_duplicates_use_the_maximum_weight` — `test_norm_memory.cpp` | Дубликаты известного корня сворачиваются максимумом, а не суммой. | полное |
| NM-010 | `nm010_hidden_source_graph_cannot_change_projected_belief` — `test_norm_memory.cpp` | Разная скрытая структура источников при одинаковой проекции не меняет belief/стоимость чтения. | полное |
| NM-011 | `nm011_new_revision_replaces_instead_of_doubling_a_root` — `test_norm_memory.cpp` | Новая revision заменяет вклад корня в исходном времени. | полное |
| NM-012 | `nm012_same_revision_with_different_meaning_is_rejected_strongly` — `test_norm_memory.cpp` | Конфликт смысла при той же revision явно отвергается без мутации. | полное |
| NM-013 | `nm013_split_exposure_matches_one_full_exposure` — `test_norm_memory.cpp` | Дробные экспозиции и одна полная доза дают один posterior. | полное |
| NM-014 | `nm014_actor_practice_context_day_cap_is_independent_of_variant` — `test_norm_memory.cpp` | Суточный cap общий для actor/practice/context и не обходится variant. | полное |
| NM-015 | `nm015_empirical_weight_halves_without_turning_prior_into_evidence` — `test_norm_memory.cpp` | Эмпирический вес распадается до 4; prior не превращается в новое событие. | полное |
| NM-016 | `nm016_predict_is_a_pure_cached_read` — `test_norm_memory.cpp` | Повторный cached predict не меняет память или время происхождения. | полное |
| NM-017 | `nm017_incomplete_reaction_window_is_unknown_not_indifference` — `test_norm_memory.cpp` | Незавершённое окно реакции остаётся неизвестным и не создаёт no-sanction/indifference. | полное |
| NM-018 | `nm018_old_known_root_is_not_rejuvenated_by_a_new_delivery` — `test_norm_memory.cpp` | Пересказ старого известного корня не омолаживает его время. | полное |
| NM-019 | `nm019_hidden_object_cannot_change_same_accessible_learning` — `engine/tests/life/test_norm_conformance.cpp` | Две копии с разной скрытой вещью получают одинаковую доступную запись и одинаковую NormMemory. | полное |
| NM-020 | `nm020_gate_zero_preserves_uninterpreted_evidence` — `engine/tests/life/test_norm_runtime.cpp` | Gate=0 оставляет запись в очереди без завершённой интерпретации и обучения. | полное |
| NM-021 | `nm021_autonomy_off_queues_without_direct_learning` — `test_norm_runtime.cpp` | При выключенной автономии допустимая запись сохраняется, но belief напрямую не меняется. | полное |
| NM-022 | `nm022_interpret_and_integrate_are_two_paid_operations` — `test_norm_runtime.cpp` | Интерпретация и интеграция — две операции; revision появляется только после второй. | полное |
| NM-023 | `nm023_budget_one_keeps_the_record_for_later` — `test_norm_conformance.cpp` | Бюджет одной операции не даёт бесплатной интеграции и не теряет запись. | полное |
| NM-024 | `nm024_stale_snapshot_cannot_overwrite_newer_memory` — `test_norm_runtime.cpp` | Изменение памяти инвалидирует активную операцию; старый результат не записывается. | полное |
| NM-025 | `nm025_unrecognized_witness_creates_no_clothing_fact` — `test_norm_runtime.cpp` | Нераспознанный свидетель не публикует clothing evidence и не создаёт membership. | полное |
| NM-026 | `nm026_observed_no_reaction_is_evidence_not_morality` — `test_norm_conformance.cpp` | Наблюдённая нулевая реакция записывается как исход, не как личная мораль. | полное |
| NM-027 | `nm027_absent_institution_produces_no_negative_sanction_evidence` — `test_norm_conformance.cpp` | Выключенный институт не создаёт отрицательное sanction-свидетельство. | полное |
| NM-028 | `nm028_hidden_membership_does_not_enter_personal_context` — `test_norm_runtime.cpp` | Скрытое изменение реестра организации не добавляет участника в личный контекст. | полное |
| NM-029 | `nm029_observing_another_does_not_reward_self_model` — `test_norm_runtime.cpp` | Наблюдение чужого поведения интегрирует норму без ложного Self update. | полное |
| NM-030 | `nm030_own_observed_outcome_can_update_self_norms_cannot` — `test_norm_conformance.cpp` | Собственный OutcomeSignal меняет SelfModel, а norm observation сам по себе — нет. | полное |
| NM-031 | `nm031_one_record_is_interpreted_and_integrated_once` — `test_norm_runtime.cpp` | Одна запись обрабатывается один раз и не будит новый decision каждую секунду. | полное |
| NM-032 | `nm032_report_keeps_reported_origin_and_the_speaker` — `engine/tests/life/test_norm_social.cpp` | Рассказ остаётся Reported с исходным speaker/root/revision. | полное |
| NM-033 | `nm033_declined_explanation_never_publishes_unheard_content` — `test_norm_social.cpp` | Отказ слушателя не публикует и не усваивает непрослушанное объяснение. | полное |
| NM-034 | `NM_034_low_motive_does_not_force_disapproval` — `engine/tests/life/test_norm_planner.cpp` | Нулевая значимость группы не создаёт команду осуждения. | полное |
| NM-035 | `NM_035_active_group_goal_can_create_approval_choice` — `test_norm_planner.cpp`; `nm035_group_goal_can_complete_an_unscripted_approval` — `test_norm_conformance.cpp` | Активная цель создаёт самостоятельный кандидат ApprovePractice; production World выбирает и завершает реакцию без сценарного назначения действия. | полное |
| NM-036 | `nm036_false_stated_rule_is_not_corrected_from_world_state` — `test_norm_social.cpp` | Ложное сообщённое правило сохраняется как субъективное и не исправляется мировой истиной. | полное |
| NM-037 | `nm037_delivery_of_unread_text_is_not_norm_knowledge` — `engine/tests/life/test_norm_phone.cpp` | Доставка непрочитанного текста не создаёт norm knowledge. | полное |
| NM-038 | `nm038_read_payload_waits_for_paid_social_and_norm_interpretation` — `test_norm_phone.cpp` | После чтения payload ждёт оплачиваемых social/norm операций. | полное |
| NM-039 | `nm039_unknown_number_cannot_be_derived_from_hidden_actor_id` — `test_norm_phone.cpp` | Скрытый actor ID не превращается в известный номер телефона. | полное |
| NM-040 | `nm040_repeated_transport_delivery_has_one_norm_contribution` — `test_norm_social.cpp` | Повтор одной transport delivery даёт один нормативный вклад. | полное |
| NM-041 | `nm041_one_speaker_does_not_create_group_consensus` — `test_norm_social.cpp` | Один speaker не создаёт доказанного согласия группы. | полное |
| NM-042 | `nm042_full_personal_principle_is_a_finite_cost` — `test_norm_memory.cpp` | Полный личный принцип даёт конечную цену и не удаляет физическую возможность. | полное |
| NM-043 | `nm043_personal_principle_does_not_require_an_audience` — `test_norm_memory.cpp` | Личное сопротивление сохраняется без аудитории. | полное |
| NM-044 | `nm044_unknown_permission_is_not_proven_absence_of_permission` — `test_norm_memory.cpp` | Unknown permission не становится доказанным отсутствием разрешения. | полное |
| NM-045 | `nm045_exception_changes_qualification_without_weakening_principle` — `test_norm_memory.cpp` | Исключение меняет применимость, сохраняя вес принципа. | полное |
| NM-046 | `nm046_descriptive_frequency_never_updates_personal_weight` — `test_norm_memory.cpp` | Описательная частота не обновляет personal weight. | полное |
| NM-047 | `nm047_only_an_explicitly_accepted_argument_updates_principle` — `test_norm_memory.cpp` | Только explicit accepted argument даёт ожидаемое w=.7940299002495008. | полное |
| NM-048 | `enabled_mode_never_reads_legacy_normative_arrays` — `test_norm_planner.cpp` | Sentinel NaN в старых `norms`/`risks` не попадает в Enabled forecast: новый ledger остаётся конечным и старый штраф не добавляется вторым источником морали. | полное |
| NM-049 | `receiver_refusal_prevents_contact_effect` — `engine/tests/life/test_social.cpp` | Без решения адресата контакт не завершается и эффект не применяется. | полное |
| NM-050 | `NM_050_duplicate_consequence_key_is_an_error` — `engine/tests/life/test_norm_ledger.cpp` | Повтор ConsequenceKey является ошибкой, а не вторым слагаемым. | полное |
| NM-051 | `NM_051_raw_terms_are_squashed_once_after_sum` — `test_norm_ledger.cpp` | Raw terms суммируются до единственного squash; проверено .6/1.6. | полное |
| NM-052 | `NM_052_sanction_uses_the_conditional_probability_chain` — `test_norm_ledger.cpp` | Санкция использует условную цепочку и даёт .096 без изменения кошелька. | полное |
| NM-053 | `NM_053_unknown_sanction_component_stays_unknown` — `test_norm_ledger.cpp` | Неизвестный компонент санкции остаётся Unknown/Assumed с источником. | полное |
| NM-054 | `NM_054_present_value_term_is_not_discounted_twice` — `test_norm_ledger.cpp` | Present value не получает второй discount. | полное |
| NM-055 | `NM_055_specific_experience_blends_instead_of_adding_a_prior` — `test_norm_ledger.cpp`; `NM_055_specific_approval_is_one_contextual_norm_outcome` — `test_norm_planner.cpp` | Конкретный опыт смешивается с обобщением и создаёт один SocialAcceptance term. | полное |
| NM-056 | `NM_056_self_prior_is_one_blended_forecast_not_a_second_cost` — `test_norm_ledger.cpp`; `NM_056_self_cost_is_residual_after_detailed_social_forecast` — `test_norm_planner.cpp` | Self prior входит один раз как остаточный прогноз, без второй полной цены отказа. | полное |
| NM-057 | `NM_057_one_person_in_two_groups_has_one_reaction_key` — `test_norm_ledger.cpp` | Один человек в двух группах даёт один итоговый reaction key. | полное |
| NM-058 | `money_urgency_does_not_allow_worse_work_interrupt` — `engine/tests/life/test_adaptive.cpp` | Механизм interruption сохраняет работу и отвергает заведомо худший вариант даже при сильном фокусе; источник фокуса не даёт разрешения обойти этот gate. | полное |
| NM-059 | `nm059_tension_has_no_immediate_physical_or_self_effect` — `test_norm_conformance.cpp` | Lockstep control показывает: вопрос о норме не меняет деньги, еду, вещи, тело или Self до действия. | полное |
| NM-060 | `NM_060_enabled_clothing_keeps_three_known_skus` — `test_norm_planner.cpp` | Enabled mode сохраняет все три известные SKU. | полное |
| NM-061 | `NM_061_clothing_condition_is_not_a_prestige_reward` — `test_norm_planner.cpp` | Цена/престиж не трактуются как выгода исправности одежды. | полное |
| NM-062 | `chosen_goals_change_financial_reserve`; `discretionary_purchase_cannot_consume_precautionary_food_buffer` — `engine/tests/life/test_civil_unit.cpp`; `nm062_unaffordable_norm_goal_waits_while_earning_remains_available` — `test_norm_conformance.cpp` | Цель увеличивает reserve; недоступная покупка не совершается, norm question сохраняется, а Work остаётся допустимым путём накопления. | полное |
| NM-063 | `NM_063_guarded_budget_keeps_the_reserve_filter` — `test_norm_planner.cpp` | Guarded policy сохраняет жёсткий reserve filter. | полное |
| NM-064 | `NM_064_deliberative_budget_prices_the_marginal_buffer` — `test_norm_planner.cpp` | Deliberative policy добавляет ровно .3 marginal buffer без запрета. | полное |
| NM-065 | `nm065_normative_purchase_has_one_future_buffer_term` — `test_norm_conformance.cpp` | Нормативная покупка содержит один future-food/buffer Resource term, без двойного ущерба. | полное |
| NM-066 | `nm066_waiting_clothing_keeps_other_known_choices` — `test_norm_conformance.cpp` | При недоступной покупке сохраняются другие известные Leisure/Rest действия. | полное |
| NM-067 | `nm067_same_revision_does_not_create_or_wake_another_question` — `test_norm_conformance.cpp` | Повтор той же revision не создаёт и не будит новую цель-вопрос. | полное |
| NM-068 | `nm068_new_revision_reopens_only_its_linked_question` — `test_norm_conformance.cpp` | Новая revision будит только связанный вопрос, не соседний. | полное |
| NM-069 | `nm069_norm_focus_keeps_each_accessible_hunger_remedy` — `test_norm_conformance.cpp` | При norm focus отдельно сохраняются доступные Eat, AcquireFood и AskMoney. | полное |
| NM-070 | `NM_070_norm_question_allows_spontaneous_known_contact` — `test_norm_planner.cpp` | Production candidate builder без назначенной встречи создаёт исполнимый Talk/AskPractice к известному доступному человеку. | полное |
| NM-071 | `nm071_context_switch_selects_local_group_and_keeps_old_belief` — `test_norm_runtime.cpp` | После смены места выбирается локальная группа, а старый belief сохраняется. | полное |
| NM-072 | `nm072_hidden_job_change_does_not_rewrite_known_membership` — `test_norm_conformance.cpp`; `nm028_hidden_membership_does_not_enter_personal_context` — `test_norm_runtime.cpp` | Скрытое изменение работы/реестра не переписывает личную группу. | полное |
| NM-073 | `nm073_zero_group_goal_creates_no_conformity_question` — `test_norm_runtime.cpp` | Нулевая цель принадлежности не создаёт conformity question; личный belief остаётся известным. | полное |
| NM-074 | `nm074_self_history_changes_residual_not_the_norm_prediction` — `test_norm_conformance.cpp` | Разный SelfModel меняет `norm_raw` и итоговый score при идентичном NormPrediction. | полное |
| NM-075 | `nm075_bootstrap_numeric_priors_have_explicit_sources` — `test_norm_conformance.cpp` | Все bootstrap priors имеют явный LegacyPrior source/root, включая sanction severity. | полное |
| NM-076 | `nm076_frequency_and_disapproval_can_both_be_high` — `test_norm_conformance.cpp` | Независимые акторы дают одновременно высокую частоту и высокое неодобрение. | полное |
| NM-077 | `nm077_relearning_is_local_and_gradual` — `test_norm_conformance.cpp` | Принятые доводы постепенно меняют только выбранный принцип без reset личности. | полное |
| NM-078 | `nm036_false_stated_rule_is_not_corrected_from_world_state` — `test_norm_social.cpp` | Субъективная ошибка сохраняется без телепатической коррекции из World. | полное |
| NM-079 | `nm079_restore_between_interpret_and_integrate_is_exactly_once` — `test_norm_persistence.cpp` | Save/load между операциями даёт тот же hash и ровно одну интеграцию/источник. | полное |
| NM-080 | `serialization_mid_send_and_unread_message` — `engine/tests/life/test_phone.cpp` | Save/load в середине отправки и до чтения сохраняет тот же будущий state hash. | полное |
| NM-081 | `nm081_hot_and_cold_records_survive_restore_and_future_paid_work` — `test_norm_persistence.cpp` | Hot/cold память переживает restore, а будущая оплаченная обработка эквивалентна. | полное |
| NM-082 | `nm082_nm083_workers_index_and_logs_do_not_change_state`; `nm082_context_compare_stage_restore_is_worker_invariant` — `test_norm_persistence.cpp`; `social_workers_preserve_exact_thoughts` — `test_social.cpp` | Реальные 1/2/4-worker и разные chunk дают одинаковый hash, точный порядок NormTrace/thought JSON; save/load внутри CompareNormAlternatives сохраняет future trace при смене 4x3 на 2x2. | полное |
| NM-083 | `nm082_nm083_workers_index_and_logs_do_not_change_state` — `test_norm_persistence.cpp` | Включение norm/event logger и смена индекса не меняют state hash. | полное |
| NM-084 | `nm084_inbox_cap_reports_drop_without_free_learning` — `test_norm_runtime.cpp` | Inbox cap=32, overflow явно counted как dropped, без бесплатной интеграции. | полное |
| NM-085 | `nm085_old_save_version_is_rejected_explicitly` — `test_norm_persistence.cpp` | Старый формат сохранения явно отвергается. | полное |
| NM-086 | `canonical_order_does_not_depend_on_insertion_order` — `test_norm_ledger.cpp`; `nm082_nm083_workers_index_and_logs_do_not_change_state` — `test_norm_persistence.cpp` | Перестановки входных ledger terms дают одинаковый канонический результат; реальные worker/chunk конфигурации возвращают один hash и один причинный trace независимо от внутреннего completion. | полное |
| NM-087 | `nm087_no_new_basis_cannot_repeat_integration` — `test_norm_persistence.cpp` | Без нового основания нет повторной интеграции, очередь пуста, время мира продолжает идти. | полное |

## Неполное покрытие

Полностью несопоставленных или известных частично покрытых строк после
усиления NM-035, NM-062, NM-082 и NM-086 нет. NM-058 проверяет общий gate
переключения, для которого источник сильного фокуса не является входом;
NM-070 проверяет production candidate builder, поскольку ожидаемый результат
матрицы требует возможности обратиться, а не обязательного согласия адресата.

Дополнительные runtime-регрессии без отдельного номера матрицы проверяют, что
подготовленный norm view истекает после смены места, памяти, контекста или 30
секунд; ответ ExplainPractice переживает посторонние Compare, дедуплицируется и
удаляется только при реальном intent; Reported PersonalPrinciple не означает
`accepted_argument`; группы 1 и 2 строятся только из собственного контракта и
лично посещённых HistoryEvent; новая revision собственного трудового контракта
создаёт новый локальный рабочий контекст без удаления старого; Reported
Approval по Help не превращается в выдуманное собственное нарушение.

Окончательный `out/norm-final-r3/tests/life_tests_all.stdout.log`:
**RESULT 582 cases, 0 failures**. Полный Release CTest: **42/42**.
Дополнительно проверены revision/subject реально полученной реакции,
точная применимость осуждения текущей ClothingTier и извлечение нормы по
готовому собственному вопросу одежды при нересурсном фокусе. Последняя
проверка исключает неизвестную группу, группу без связи с собственной целью
и Help из той же будущей группы. Порядок равнозначных ClothingTier проверен
при известном, неизвестном и нулевом Detection, видимой аудитории,
неприменимости, исключении, нулевых ценностях и преобладающей санкции.
Эти дополнительные случаи не заменяют пяти причинных серий World.
