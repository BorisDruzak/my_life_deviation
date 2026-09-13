# Примеры информации, памяти и мыслей 0.10.0

Это выдержки из фактических JSON/JSONL окончательной сборки. Русский текст журнала — представление структурированной операции, не отдельная LLM.

## Передача в группе
В авторской начальной сцене разговоры уже заданы. Утверждение о работе НПС8 первоначально знает только НПС1. Фикстура не доказывает истинность утверждения: проверяется его передача.

```json
{
  "ms": 2624,
  "started_ms": 2457,
  "actor": 1,
  "thought": 6,
  "episode": 1,
  "focus": 4000224,
  "kind": "forecast",
  "origin": 3,
  "status": 1,
  "metric": "общение",
  "person": 2,
  "method": "social",
  "destination": 2,
  "moral_cost": 0,
  "risk_cost": 0,
  "resource_cost": 0,
  "time_cost": 0,
  "object": 226,
  "interaction": "share_news",
  "expected_pleasure": 0.3,
  "expected_acceptance": 0.95,
  "expected_status_gain": 0,
  "knowledge_source": 227,
  "basis": 5,
  "own_revision": 1,
  "value": 0.319125163233383,
  "confidence": 0.8,
  "observation": 0,
  "prior": 0,
  "spent": 4,
  "slots": 3,
  "detail": "social_forecast_from_own_knowledge_attitude_norms",
  "text": "Рассматриваю «рассказать сведения»: ожидаемая приятность 0.3, собственная моральная цена 0, оценка 0.319"
}
```

```json
{
  "ms": 2958,
  "started_ms": 2791,
  "actor": 1,
  "thought": 8,
  "episode": 1,
  "focus": 4000224,
  "kind": "intent",
  "origin": 2,
  "status": 1,
  "metric": "общение",
  "person": 2,
  "method": "social",
  "destination": 2,
  "moral_cost": 0,
  "risk_cost": 0,
  "resource_cost": 0,
  "time_cost": 0,
  "object": 226,
  "interaction": "share_news",
  "expected_pleasure": 0,
  "expected_acceptance": 0,
  "expected_status_gain": 0,
  "knowledge_source": 0,
  "basis": 6,
  "own_revision": 1,
  "value": 0.319125163233383,
  "confidence": 0.8,
  "observation": 0,
  "prior": 0,
  "spent": 6,
  "slots": 2,
  "detail": "команда_исполнителю",
  "text": "Выбрал «рассказать сведения» по завершённому прогнозу 6; команда_исполнителю"
}
```

```json
{
  "ms": 5474,
  "started_ms": 5271,
  "actor": 2,
  "thought": 8,
  "episode": 3,
  "focus": 281474976710885,
  "kind": "interpret_reply",
  "origin": 2,
  "status": 1,
  "metric": "общение",
  "person": 1,
  "method": "social",
  "destination": 0,
  "moral_cost": 0,
  "risk_cost": 0,
  "resource_cost": 0,
  "time_cost": 0,
  "object": 226,
  "interaction": "share_news",
  "expected_pleasure": 0.3,
  "expected_acceptance": 1,
  "expected_status_gain": 0,
  "knowledge_source": 217,
  "basis": 229,
  "own_revision": 2,
  "value": 0.30536440382461444,
  "confidence": 0.8,
  "observation": 0,
  "prior": 0,
  "spent": 1,
  "slots": 4,
  "detail": "social_accept_specific_proposal",
  "text": "Рассматриваю «рассказать сведения»: ожидаемая приятность 0.3, собственная моральная цена 0, оценка 0.305"
}
```

```json
{
  "ms": 6847,
  "started_ms": 6680,
  "actor": 1,
  "thought": 11,
  "episode": 3,
  "focus": 281474976710890,
  "kind": "interpret_reply",
  "origin": 0,
  "status": 1,
  "metric": "общение",
  "person": 2,
  "method": "social",
  "destination": 0,
  "moral_cost": 0,
  "risk_cost": 0,
  "resource_cost": 0,
  "time_cost": 0,
  "object": 226,
  "interaction": "share_news",
  "expected_pleasure": 0,
  "expected_acceptance": 0,
  "expected_status_gain": 0,
  "knowledge_source": 0,
  "basis": 234,
  "own_revision": 4,
  "value": 0,
  "confidence": 0.8,
  "observation": 0,
  "prior": 0,
  "spent": 1,
  "slots": 4,
  "detail": "social_observed_share_news_none",
  "text": "рассказать сведения; social_observed_share_news_none; пережитый/рассчитанный результат 0; предмет 226"
}
```

```json
{
  "ms": 66824,
  "started_ms": 66631,
  "actor": 4,
  "thought": 11,
  "episode": 5,
  "focus": 281474976710894,
  "kind": "interpret_reply",
  "origin": 0,
  "status": 1,
  "metric": "общение",
  "person": 1,
  "method": "social",
  "destination": 0,
  "moral_cost": 0,
  "risk_cost": 0,
  "resource_cost": 0,
  "time_cost": 0,
  "object": 0,
  "interaction": "share_news",
  "expected_pleasure": 0,
  "expected_acceptance": 0,
  "expected_status_gain": 0,
  "knowledge_source": 0,
  "basis": 238,
  "own_revision": 3,
  "value": 0,
  "confidence": 0.8,
  "observation": 0,
  "prior": 0,
  "spent": 1,
  "slots": 3,
  "detail": "social_observed_share_news_none",
  "text": "рассказать сведения; social_observed_share_news_none; пережитый/рассчитанный результат 0; предмет 0"
}
```

```json
{
  "ms": 66846,
  "started_ms": 66679,
  "actor": 1,
  "thought": 16,
  "episode": 6,
  "focus": 281474976710891,
  "kind": "interpret_reply",
  "origin": 0,
  "status": 1,
  "metric": "общение",
  "person": 2,
  "method": "social",
  "destination": 0,
  "moral_cost": 0,
  "risk_cost": 0,
  "resource_cost": 0,
  "time_cost": 0,
  "object": 226,
  "interaction": "share_news",
  "expected_pleasure": 0,
  "expected_acceptance": 0,
  "expected_status_gain": 0,
  "knowledge_source": 0,
  "basis": 235,
  "own_revision": 7,
  "value": 0.2509202726962755,
  "confidence": 0.8,
  "observation": 0,
  "prior": 0,
  "spent": 1,
  "slots": 4,
  "detail": "social_observed_share_news_none",
  "text": "рассказать сведения; social_observed_share_news_none; пережитый/рассчитанный результат 0.251; предмет 226"
}
```

```json
{
  "ms": 66948,
  "started_ms": 66745,
  "actor": 2,
  "thought": 13,
  "episode": 6,
  "focus": 281474976710892,
  "kind": "interpret_reply",
  "origin": 0,
  "status": 1,
  "metric": "общение",
  "person": 1,
  "method": "social",
  "destination": 0,
  "moral_cost": 0,
  "risk_cost": 0,
  "resource_cost": 0,
  "time_cost": 0,
  "object": 226,
  "interaction": "share_news",
  "expected_pleasure": 0,
  "expected_acceptance": 0,
  "expected_status_gain": 0,
  "knowledge_source": 0,
  "basis": 236,
  "own_revision": 5,
  "value": 0.250624306917238,
  "confidence": 0.8,
  "observation": 0,
  "prior": 0,
  "spent": 1,
  "slots": 4,
  "detail": "social_observed_share_news_none",
  "text": "рассказать сведения; social_observed_share_news_none; пережитый/рассчитанный результат 0.251; предмет 226"
}
```

```json
{
  "ms": 67012,
  "started_ms": 66827,
  "actor": 3,
  "thought": 8,
  "episode": 4,
  "focus": 281474976710893,
  "kind": "interpret_reply",
  "origin": 0,
  "status": 1,
  "metric": "общение",
  "person": 1,
  "method": "social",
  "destination": 0,
  "moral_cost": 0,
  "risk_cost": 0,
  "resource_cost": 0,
  "time_cost": 0,
  "object": 0,
  "interaction": "share_news",
  "expected_pleasure": 0,
  "expected_acceptance": 0,
  "expected_status_gain": 0,
  "knowledge_source": 0,
  "basis": 237,
  "own_revision": 4,
  "value": 0,
  "confidence": 0.8,
  "observation": 0,
  "prior": 0,
  "spent": 1,
  "slots": 3,
  "detail": "social_observed_share_news_none",
  "text": "рассказать сведения; social_observed_share_news_none; пережитый/рассчитанный результат 0; предмет 0"
}
```

## Содержание осталось, место и время забыты
Фактическая память НПС1 в недельном мире seed42. origin=2 означает Reported; нулевое место и время -1 — неизвестные детали, не координата/дата происшествия:
```json
{
  "id": 362,
  "kind": 1,
  "subject": 3,
  "speaker": 2,
  "origin": 2,
  "cited_source": 3,
  "location": 0,
  "occurred_at": -1,
  "encoded_at": 66357358,
  "strength": 0.195859750985,
  "disclosure": 0
}
```

Speaker — последний известный говорящий/служебная запись источника доставки. Сохранение технической записи источника не означает её доступность для речи: публичная проекция cited_source может быть 0. Общий граф независимости слухов не реализован.

## Диагностический тупик до исправления
В development-trace/stuck12-thoughts.jsonl НПС12 снова рассматривает сон, прогнозирует -0.012518 и выбирает ожидание. Это реальная старая трасса test1, **не финальная модель**. Регрессия 31-seed7-focus-red.log воспроизводит проблему; 32-focus-green.log показывает её исправление.
