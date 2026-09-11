# 4. Отчёты: потребности, отказы и причины выбора

**Статус:** проект REPORTS-0.1. Требования RP-*. Источником объяснения служит фактическое вычисление контроллера, а не последующая выдуманная история.

## 4.1. Три уровня видимости

RP-01: разделить `developer_omniscient` (полный симуляционный аудит), `actor_private` (только собственное объяснение и полученные факты), `communicated` (действительно произнесённое/прочитанное). Эти экспорты нельзя автоматически превращать в общий канал знаний NPC или интерфейс расследующего игрока.

Пример: адресат отказал из-за собственного дефицита отдыха; сказал «сегодня занят»; инициатор прочитал только эту фразу. В авторском отчёте возможны все три записи с разными ACL. В LocalView инициатора нет ни N02 адресата, ни истинного score, ни доказательства, что фраза правдива. Его `inferred_cause` может быть unknown или гипотезой с источником.

## 4.2. Идентификаторы и происхождение

```text
run_id, world_version, policy_version, config_hash, scenario_hash,
actor_id, tick, boundary_phase, decision_id, goal_id, plan_id,
candidate_id, command_key, receipt_id, proposal_id, proposal_version,
session_id, need_episode_id, cause_ref[], source_evidence_ref[]
```

RP-02: каждое решение получает ID, включая no-command. Последующие команда и receipt ссылаются на этот ID. Технический reject не переписывает исходную причину выбора. Ключи фиксируются из локального sequence, а не wall clock или случайного UUID; replay сохраняет смысловую тождественность. Повторное чтение/импорт одного root не создаёт нового отказа или эффекта.

DecisionTrace записывается **в момент принятия решения** из фактически использованных оценок. Подбор объяснения по результату события запрещён. Отдельная запись OutcomeLink соединяет прогноз и результат, не редактируя прошлое.

## 4.3. Потребности: источник данных

RP-03: до/после каждой физической минуты `apply_minute` отдаёт инструментированному sink значения и разложение без изменения расчёта:

```text
NeedDelta { actor, need, tick, enabled, alive_at_start,
  before, passive_drift_raw, effect_components_raw[{cause_ref,amount}],
  clamp_adjustment, after, active_before, active_after,
  activation, critical, target }
```

Точная тождественность в машинной погрешности:
`after-before = passive_drift_raw + sum(effect_components_raw) + clamp_adjustment`.
`clamp_adjustment=clip(raw_after,0,100)-raw_after`. При насыщении нельзя раздать весь эффект каждому параллельному источнику; поправка насыщения остаётся отдельной строкой. Смерть/выключение потребности не маскируются её восстановлением. Переходы active-защёлки не заменяют числовое значение.

MinuteDelta можно агрегировать потоково и не хранить все минуты в JSONL. Точность метрик при этом сохраняется. Выключенный sink не должен менять ветвления физики, случайные числа, порядок actors или результаты вычислений.

## 4.4. Окно и формулы агрегатов

RP-04: окно `[begin,end)` содержит `end-begin` интервалов. Значение на левой границе n[t] описывает экспозицию минуты `[t,t+1)`. Финальный снимок n[end] — конечное состояние, а не дополнительная минута. Ключ `(actor,need,tick)` уникален; sample в конце не дублируется.

Пусть E — минуты, в начале которых потребность enabled и NPC alive. Incapable, но живой NPC остаётся в E: его дефицит нельзя исключить для красивой средней. Для скорости решений отдельный знаменатель — допустимые decision opportunities.

```text
mean_time_weighted = sum(t in E, n[t]) / |E|
below_activation_minutes = sum(t in E, n[t] < A[t])
below_critical_minutes = sum(t in E, n[t] <= C[t])
active_minutes = sum(t in E, active[t])
deficit_point_minutes = sum(t in E, max(0,T[t]-n[t]))
normalized_deficit_minutes = sum(t in E, max(0,(T[t]-n[t])/(T[t]-C[t])))
```

При E пустом среднее/доли — null с `exposure_minutes=0`, а не ноль. Отдельно min/max по наблюдаемым границам, start/end, net_change, passive_drift_sum, эффект по cause family, clamp_sum, число эпизодов, среднее/квантили длительности **завершённых** эпизодов и количество незавершённых.

Поле `active_minutes` не равно below_activation_minutes: гистерезис сохраняет активность до target. Для выключенной N05 — запись enabled=false без нулевой «плохой удовлетворённости» в среднем по потребностям.

RP-05: NeedEpisode начинается переходом неактивной защёлки в активную. Если окно стартовало с активной потребностью — left_censored=true, onset_tick неизвестен при отсутствии сохранённой истории. Если target не достигнут к концу — right_censored=true, observed_duration известна, полная duration=null. Смерть/выключение завершают наблюдение с отдельным terminal_reason, не `recovered`.

В эпизоде сохраняются first_goal_tick, first_attempt_tick, first_actual_effect_tick и source decision/receipt. Отсутствие эффекта классифицируется по цепочке фактически наблюдавшихся стадий, а не списывается на последнюю видимую команду.

## 4.5. Разные значения слова «отказ»

| family | Что произошло | Ключ агрегации и знаменатель |
|---|---|---|
| candidate_filtered | Сам NPC не стал выполнять оценённый вариант | decision_id/candidate_id; число реально рассмотренных вариантов |
| social_declined | Прочитанный адресат явно отказал текущей версии | proposal/version/responder; финальные явные ответы того же scope |
| execution_rejected | World отверг реально поданную команду | command_key; все поданные команды соответствующего типа |
| negotiation_timeout | Нет нужного ответа до срока | proposal/version и собственный known status; все созданные предложения, отдельно delivery/read |
| cancelled / interrupted / missed_window | Намерение/занятие прекращено либо не началось вовремя | session_id; число согласованных сессий или реально начатых для interruption |

RP-06: timeout и execution_rejected не входят в долю социальных отказов. Есть две разные метрики: доля ответов `declined/(accepted+declined)` на уникальные финальные ответы и доля предложений, завершившихся явным отказом. Для групп результаты участников не суммируются как несколько разных предложений. Counter показывается отдельно; промежуточные ответы старой версии не входят в финальную воронку последней версии.

В developer отчёте доступен фактический ответ даже до его прочтения инициатором, но метрика должна иметь scope=world_response. В метрике actor_knowledge отказ появляется только после доступного получения/прочтения; два scope никогда не смешиваются в одном знаменателе.

## 4.6. Справочник причин

RP-07: причины — типизированные code, stage, privacy, source, certainty. Минимальный словарь:

- Собственный выбор: `need_not_active`, `low_expected_gain`, `dominated_by_alternative`, `critical_self_need`, `fatigue_risk`, `calendar_conflict`, `insufficient_budget`, `not_interested`, `cooldown_active`.
- Доступность метода: `method_unknown`, `partner_unknown`, `contact_unknown`, `route_unknown`, `resource_known_unavailable`, `permission_unknown`, `topic_unregistered`, `stale_or_conflicting_evidence`.
- Поиск: `not_evaluated_budget`, `depth_limit`, `projection_limit`, `route_budget_exhausted`, `no_known_method`, `known_infeasible`, `unknown_blocker`.
- Исполнение/протокол: `conditions_not_met` (сохранить публичное обобщение ядра), `message_not_read`, `awaiting_response`, `readiness_pending` (локальная гипотеза), `withdrawn`, `terminal_session`, `window_closed`.

Одновременно допустимы несколько причин, но primary_reason определяется стадией первого решающего ограничения: собственная безопасность → невозможное предусловие → временной конфликт → отрицательная полезность → стабильное разрешение равенства. Неизвестный ресурс не помечается как доказанно отсутствующий.

`private_reason` — действительная причина расчёта этого NPC; `communicated_reason` — связанное реальное сообщение; `inferred_reason` — вывод конкретного получателя; `world_validation_detail` — закрытая диагностика исполнителя. Для каждой причины обязателен origin. Отсутствующие значения null, а не придуманный стандартный мотив.

## 4.7. Полный DecisionTrace

RP-08: обязательные поля:

```text
identity: decision_id, actor, tick, policy_version, local_view_hash
trigger: periodic | observation | receipt | need_emergency | deadline
own_state: enabled_needs + thresholds, fatigue, stress, own available budget
knowledge: used_source_refs, assumptions, freshness, conflicts
search: limits, used, stop_reason, coverage=bounded, omitted_counts
objectives: goal_id, family, status, urgency, blocker, source
candidates: id, goal_ids, method, steps_summary, preconditions,
  horizon_minutes, predicted_effects, numeric_score_components,
  uncertainty_kind, feasibility, exclusion_reason
choice: candidate_id|null, runner_up_id|null, score_gap|null,
  selection_rule, no_command_reason|null, next_review_tick
links: active_plan_id, proposal/version, emitted_command_keys
```

Числовое разложение воспроизводит сумму Score; сравнимость кандидатов требует одинакового горизонта и версии модели. Величина score_gap существует только для оценённых допустимых альтернатив. Не вычисленная альтернатива имеет score=null, не 0 и не «хуже». Статус exhausted не означает оптимальное решение.

При trace level=summary сохраняются выбранный, лучший оценённый конкурент и счётчики фильтров; level=full — все рассмотренные варианты в рамках бюджета. Нельзя отключением full ускорить сам search или изменить выбор. Наличие записи no-command важно для активных unmet needs и waiting_external; не нужно превращать каждую idle-минуту в полноценный дорогой search.

RP-09: ответ «почему не выбрано X» сначала проверяет, рассматривался ли X. Если нет — `not_evaluated` и конкретный лимит/неизвестность. Дополнительный офлайн эксперимент с X явно называется counterfactual analysis, имеет другую модель/контекст и не подменяет исходную причину.

## 4.8. Воронка социальной активности

Считать отдельно по actor/need episode и proposal/session:
`need_active → social_goal_generated → method_found → candidate_evaluated → proposal_created → delivered → read → response → unanimous_agreement → arrived → independently_ready → joint_started → effect_minutes → completed/interrupted`.

RP-10: `read` измеряется реальной A24/доступным очным восприятием, а не фактом нахождения сообщения в inbox. `effect_minutes` — сумма реального участия; actor-minutes и session-minutes — разные столбцы. Пример: двое общались 10 минут → 10 session-minutes, 20 actor-minutes, не дважды 20. Attendance prediction сверяется только после известного результата.

Воронка должна позволять объяснить разницу «никто не пригласил», «не было канала», «отказали», «не встретились» и «встретились, но из-за повторения эффект мал». Нельзя целевой метрикой считать максимальную долю согласий: добровольный обоснованный отказ — корректное поведение.

## 4.9. Файлы и режимы

| Файл | Содержание |
|---|---|
| report_manifest.json | Версии, хеши, window, scope, source_mode, полнота и цензурирование |
| need_windows.csv | Один actor/need/window, точные агрегаты и знаменатели |
| need_episodes.jsonl | История эпизодов, первые решения/эффекты, блокировки |
| decision_trace.jsonl | Что было рассчитано и почему выбран этот план/бездействие |
| refusals.jsonl | Независимые классы отказов/прерываний с приватностью и scope |
| social_funnel.json | Стадии, ответы, посещаемость, эффект, дедупликация версий |
| planner_summary.json | Вызовы, расширения/лимиты, исчерпания, смены планов, глубины |

RP-11: текущие `events.jsonl` и `summary.json` сохраняются для совместимости, но не объявляются полным новым отчётом. Для старых почасовых CSV возможен только source_mode=sampled, без точных below-critical minutes и причинных выводов. Старые raw event counts не переименовываются задним числом.

RP-12: агрегаты потоковые; состояние репортера имеет checkpoint cursor для exactly-once экспозиции. При недоступном пути/сбое записи запуск либо явно завершается ошибкой отчётности, либо помечает отчёт incomplete с потерянным диапазоном — не «всё успешно». Сами решения/World не получают доступ к накопленным глобальным метрикам. Отчётность on/off обязана давать одинаковый физический state hash.
