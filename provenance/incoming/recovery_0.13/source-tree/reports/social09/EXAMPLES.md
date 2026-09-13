# C++0.9.0 — реальные мысли и записи памяти

Русский текст — детерминированное отображение вычисленных данных, не дополнительный ИИ.
Исходные JSONL и personal/world state находятся в evidence/social09/runs.

## personal-boundaries

| Время, с | НПС/мысль | Способ | Ожидаемая приятность | Моральная цена | Оценка | Источник знания |
|---:|---|---|---:|---:|---:|---:|
| 17.318 | 1/36 | friendly_touch | 0.604060 | 0.000000 | 0.270500 | 138 |
| 17.789 | 1/39 | romantic_touch | 0.900000 | 1.000000 | -0.348950 | 139 |

> Рассматриваю «дружеское объятие»: ожидаемая приятность 0.604, собственная моральная цена 0, оценка 0.27

> Рассматриваю «романтическое объятие»: ожидаемая приятность 0.9, собственная моральная цена 1, оценка -0.349

```json
[
  {
    "ms": 17318,
    "started_ms": 17161,
    "actor": 1,
    "thought": 36,
    "episode": 9,
    "focus": 6,
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
    "object": 0,
    "interaction": "friendly_touch",
    "expected_pleasure": 0.6040595075312591,
    "expected_acceptance": 0.8999999999999999,
    "expected_status_gain": 0,
    "knowledge_source": 138,
    "basis": 35,
    "own_revision": 7,
    "value": 0.2704996089040251,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «дружеское объятие»: ожидаемая приятность 0.604, собственная моральная цена 0, оценка 0.27"
  },
  {
    "ms": 17789,
    "started_ms": 17632,
    "actor": 1,
    "thought": 39,
    "episode": 9,
    "focus": 6,
    "kind": "forecast",
    "origin": 3,
    "status": 1,
    "metric": "общение",
    "person": 2,
    "method": "social",
    "destination": 2,
    "moral_cost": 1,
    "risk_cost": 0,
    "resource_cost": 0,
    "time_cost": 0,
    "object": 0,
    "interaction": "romantic_touch",
    "expected_pleasure": 0.9,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0,
    "knowledge_source": 139,
    "basis": 38,
    "own_revision": 7,
    "value": -0.34895029392026167,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 9,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «романтическое объятие»: ожидаемая приятность 0.9, собственная моральная цена 1, оценка -0.349"
  }
]
```

## recipient-refuses

| Время, с | НПС/мысль | Способ | Ожидаемая приятность | Моральная цена | Оценка | Источник знания |
|---:|---|---|---:|---:|---:|---:|
| 5.982 | 1/8 | romantic_touch | 0.900000 | 0.000000 | 0.317063 | 138 |
| 8.394 | 1/16 | romantic_touch | 0.900000 | 0.000000 | 0.317039 | 138 |

> Рассматриваю «романтическое объятие»: ожидаемая приятность 0.9, собственная моральная цена 0, оценка 0.317

> Рассматриваю «романтическое объятие»: ожидаемая приятность 0.9, собственная моральная цена 0, оценка 0.317

```json
[
  {
    "ms": 5982,
    "started_ms": 5825,
    "actor": 1,
    "thought": 8,
    "episode": 2,
    "focus": 6,
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
    "object": 0,
    "interaction": "romantic_touch",
    "expected_pleasure": 0.9,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0,
    "knowledge_source": 138,
    "basis": 7,
    "own_revision": 2,
    "value": 0.3170626691784272,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «романтическое объятие»: ожидаемая приятность 0.9, собственная моральная цена 0, оценка 0.317"
  },
  {
    "ms": 8394,
    "started_ms": 8237,
    "actor": 1,
    "thought": 16,
    "episode": 3,
    "focus": 6,
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
    "object": 0,
    "interaction": "romantic_touch",
    "expected_pleasure": 0.9,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0,
    "knowledge_source": 138,
    "basis": 15,
    "own_revision": 3,
    "value": 0.3170385072279745,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «романтическое объятие»: ожидаемая приятность 0.9, собственная моральная цена 0, оценка 0.317"
  }
]
```

## borrow

| Время, с | НПС/мысль | Способ | Ожидаемая приятность | Моральная цена | Оценка | Источник знания |
|---:|---|---|---:|---:|---:|---:|
| 6.261 | 1/5 | ask_information | 0.150000 | 0.000000 | 0.336485 | 140 |
| 21.208 | 1/35 | borrow_item | 0.150000 | 0.000000 | 0.361277 | 141 |
| 221.471 | 1/165 | return_item | 0.150000 | 0.000000 | 0.156385 | 142 |
| 221.942 | 1/168 | compliment | 0.500000 | 0.000000 | 0.097585 | 184 |

> Рассматриваю «спросить сведения»: ожидаемая приятность 0.15, собственная моральная цена 0, оценка 0.336

> Рассматриваю «попросить вещь взаймы»: ожидаемая приятность 0.15, собственная моральная цена 0, оценка 0.361

> Рассматриваю «вернуть вещь»: ожидаемая приятность 0.15, собственная моральная цена 0, оценка 0.156

> Рассматриваю «комплимент»: ожидаемая приятность 0.5, собственная моральная цена 0, оценка 0.0976

```json
[
  {
    "ms": 6261,
    "started_ms": 6104,
    "actor": 1,
    "thought": 5,
    "episode": 2,
    "focus": 844424930133968,
    "kind": "forecast",
    "origin": 3,
    "status": 1,
    "metric": "досуг",
    "person": 2,
    "method": "social",
    "destination": 2,
    "moral_cost": 0,
    "risk_cost": 0,
    "resource_cost": 0,
    "time_cost": 0,
    "object": 2000,
    "interaction": "ask_information",
    "expected_pleasure": 0.15,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0,
    "knowledge_source": 140,
    "basis": 4,
    "own_revision": 2,
    "value": 0.3364845047630435,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 3,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «спросить сведения»: ожидаемая приятность 0.15, собственная моральная цена 0, оценка 0.336"
  },
  {
    "ms": 21208,
    "started_ms": 21051,
    "actor": 1,
    "thought": 35,
    "episode": 6,
    "focus": 844424930133968,
    "kind": "forecast",
    "origin": 3,
    "status": 1,
    "metric": "досуг",
    "person": 2,
    "method": "social",
    "destination": 2,
    "moral_cost": 0,
    "risk_cost": 0,
    "resource_cost": 0,
    "time_cost": 0,
    "object": 2000,
    "interaction": "borrow_item",
    "expected_pleasure": 0.15,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0,
    "knowledge_source": 141,
    "basis": 34,
    "own_revision": 8,
    "value": 0.3612774310902948,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 3,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «попросить вещь взаймы»: ожидаемая приятность 0.15, собственная моральная цена 0, оценка 0.361"
  },
  {
    "ms": 221471,
    "started_ms": 221314,
    "actor": 1,
    "thought": 165,
    "episode": 21,
    "focus": 844424930133968,
    "kind": "forecast",
    "origin": 3,
    "status": 1,
    "metric": "досуг",
    "person": 2,
    "method": "social",
    "destination": 2,
    "moral_cost": 0,
    "risk_cost": 0,
    "resource_cost": 0,
    "time_cost": 0,
    "object": 2000,
    "interaction": "return_item",
    "expected_pleasure": 0.15,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0,
    "knowledge_source": 142,
    "basis": 164,
    "own_revision": 24,
    "value": 0.15638508574059656,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 3,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «вернуть вещь»: ожидаемая приятность 0.15, собственная моральная цена 0, оценка 0.156"
  },
  {
    "ms": 221942,
    "started_ms": 221785,
    "actor": 1,
    "thought": 168,
    "episode": 21,
    "focus": 844424930133968,
    "kind": "forecast",
    "origin": 3,
    "status": 1,
    "metric": "досуг",
    "person": 2,
    "method": "social",
    "destination": 2,
    "moral_cost": 0,
    "risk_cost": 0,
    "resource_cost": 0,
    "time_cost": 0,
    "object": 0,
    "interaction": "compliment",
    "expected_pleasure": 0.5,
    "expected_acceptance": 0.7,
    "expected_status_gain": 0.25,
    "knowledge_source": 184,
    "basis": 167,
    "own_revision": 24,
    "value": 0.09758528910886409,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «комплимент»: ожидаемая приятность 0.5, собственная моральная цена 0, оценка 0.0976"
  }
]
```

## status

| Время, с | НПС/мысль | Способ | Ожидаемая приятность | Моральная цена | Оценка | Источник знания |
|---:|---|---|---:|---:|---:|---:|
| 22.080 | 1/22 | claim_item | 0.300000 | 0.000000 | 0.381540 | 139 |
| 36.179 | 1/44 | claim_item | 0.255068 | 0.000000 | 0.321858 | 139 |

> Рассматриваю «выдать вещь за свою»: ожидаемая приятность 0.3, собственная моральная цена 0, оценка 0.382

> Рассматриваю «выдать вещь за свою»: ожидаемая приятность 0.255, собственная моральная цена 0, оценка 0.322

```json
[
  {
    "ms": 22080,
    "started_ms": 21923,
    "actor": 1,
    "thought": 22,
    "episode": 8,
    "focus": 6,
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
    "object": 2000,
    "interaction": "claim_item",
    "expected_pleasure": 0.3,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0.95,
    "knowledge_source": 139,
    "basis": 21,
    "own_revision": 10,
    "value": 0.38154023839812184,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «выдать вещь за свою»: ожидаемая приятность 0.3, собственная моральная цена 0, оценка 0.382"
  },
  {
    "ms": 36179,
    "started_ms": 36022,
    "actor": 1,
    "thought": 44,
    "episode": 13,
    "focus": 562949953421453,
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
    "object": 2000,
    "interaction": "claim_item",
    "expected_pleasure": 0.2550677150248284,
    "expected_acceptance": 0.9333333333333332,
    "expected_status_gain": 0.8031244430409605,
    "knowledge_source": 139,
    "basis": 43,
    "own_revision": 18,
    "value": 0.3218582219455984,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «выдать вещь за свою»: ожидаемая приятность 0.255, собственная моральная цена 0, оценка 0.322"
  }
]
```

## status-norm

| Время, с | НПС/мысль | Способ | Ожидаемая приятность | Моральная цена | Оценка | Источник знания |
|---:|---|---|---:|---:|---:|---:|
| 22.080 | 1/22 | claim_item | 0.300000 | 1.000000 | -0.276976 | 139 |
| 27.179 | 1/31 | claim_item | 0.300000 | 1.000000 | -0.277033 | 139 |

> Рассматриваю «выдать вещь за свою»: ожидаемая приятность 0.3, собственная моральная цена 1, оценка -0.277

> Рассматриваю «выдать вещь за свою»: ожидаемая приятность 0.3, собственная моральная цена 1, оценка -0.277

```json
[
  {
    "ms": 22080,
    "started_ms": 21923,
    "actor": 1,
    "thought": 22,
    "episode": 8,
    "focus": 6,
    "kind": "forecast",
    "origin": 3,
    "status": 1,
    "metric": "общение",
    "person": 2,
    "method": "social",
    "destination": 2,
    "moral_cost": 1,
    "risk_cost": 0,
    "resource_cost": 0,
    "time_cost": 0,
    "object": 2000,
    "interaction": "claim_item",
    "expected_pleasure": 0.3,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0.95,
    "knowledge_source": 139,
    "basis": 21,
    "own_revision": 10,
    "value": -0.2769759887949482,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «выдать вещь за свою»: ожидаемая приятность 0.3, собственная моральная цена 1, оценка -0.277"
  },
  {
    "ms": 27179,
    "started_ms": 27022,
    "actor": 1,
    "thought": 31,
    "episode": 10,
    "focus": 562949953421453,
    "kind": "forecast",
    "origin": 3,
    "status": 1,
    "metric": "общение",
    "person": 2,
    "method": "social",
    "destination": 2,
    "moral_cost": 1,
    "risk_cost": 0,
    "resource_cost": 0,
    "time_cost": 0,
    "object": 2000,
    "interaction": "claim_item",
    "expected_pleasure": 0.3,
    "expected_acceptance": 0.9,
    "expected_status_gain": 0.95,
    "knowledge_source": 139,
    "basis": 30,
    "own_revision": 12,
    "value": -0.27703310862661984,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «выдать вещь за свою»: ожидаемая приятность 0.3, собственная моральная цена 1, оценка -0.277"
  }
]
```

## pleasure-mismatch

| Время, с | НПС/мысль | Способ | Ожидаемая приятность | Моральная цена | Оценка | Источник знания |
|---:|---|---|---:|---:|---:|---:|
| 16.892 | 1/15 | romantic_touch | 0.760479 | 0.000000 | 0.295820 | 138 |
| 300.942 | 1/188 | romantic_touch | 0.402283 | 0.000000 | 0.115475 | 138 |

> Рассматриваю «романтическое объятие»: ожидаемая приятность 0.76, собственная моральная цена 0, оценка 0.296

> Рассматриваю «романтическое объятие»: ожидаемая приятность 0.402, собственная моральная цена 0, оценка 0.115

```json
[
  {
    "ms": 16892,
    "started_ms": 16735,
    "actor": 1,
    "thought": 15,
    "episode": 5,
    "focus": 562949953421452,
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
    "object": 0,
    "interaction": "romantic_touch",
    "expected_pleasure": 0.7604788278460726,
    "expected_acceptance": 0.8999999999999999,
    "expected_status_gain": 0,
    "knowledge_source": 138,
    "basis": 14,
    "own_revision": 7,
    "value": 0.29581968943907366,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «романтическое объятие»: ожидаемая приятность 0.76, собственная моральная цена 0, оценка 0.296"
  },
  {
    "ms": 300942,
    "started_ms": 300785,
    "actor": 1,
    "thought": 188,
    "episode": 36,
    "focus": 562949953421452,
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
    "object": 0,
    "interaction": "romantic_touch",
    "expected_pleasure": 0.40228326602806325,
    "expected_acceptance": 0.96,
    "expected_status_gain": 0,
    "knowledge_source": 138,
    "basis": 187,
    "own_revision": 36,
    "value": 0.11547500034985701,
    "confidence": 0.8,
    "observation": 0,
    "prior": 0,
    "spent": 6,
    "slots": 3,
    "detail": "social_forecast_from_own_knowledge_attitude_norms",
    "text": "Рассматриваю «романтическое объятие»: ожидаемая приятность 0.402, собственная моральная цена 0, оценка 0.115"
  }
]
```

## Реальная последовательность работы с книгой

| Секунды | Результат | Предмет | Событие | Родительская беседа |
|---:|---|---:|---:|---:|
| 14.673 | ask_information | 2000 | 155 | 148 |
| 28.620 | borrow_item | 2000 | 160 | 148 |
| 91.555 | use_item | 2000 | 167 | 0 |
| 154.439 | use_item | 2000 | 177 | 0 |
| 228.882 | return_item | 2000 | 196 | 194 |

### borrow: фактическая память НПС1 после прогона

```json
{
  "knowledge": {
    "interaction": "compliment",
    "mastery": 0.6043098607132364,
    "expected_pleasure": 0.5,
    "expected_acceptance": 0.7,
    "confidence": 0.8,
    "source": 184,
    "debug_primary_response": 0.4
  },
  "item": [
    {
      "id": 2000,
      "owner": 2,
      "holder": 2,
      "owner_status": "confirmed",
      "holder_status": "confirmed",
      "origin": "agreement",
      "source": 199,
      "expected_use": 0.6,
      "prestige": 0.3,
      "uses": 2,
      "claims": [
        {
          "source": 158,
          "speaker": 2,
          "owner": 2,
          "holder": 2,
          "origin": "reported",
          "at": 14673
        },
        {
          "source": 163,
          "speaker": 2,
          "owner": 2,
          "holder": 1,
          "origin": "agreement",
          "at": 28620
        },
        {
          "source": 174,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 91555
        },
        {
          "source": 184,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 154439
        },
        {
          "source": 199,
          "speaker": 2,
          "owner": 2,
          "holder": 2,
          "origin": "agreement",
          "at": 228882
        }
      ]
    }
  ],
  "loan": [
    {
      "id": 160,
      "object": 2000,
      "lender": 2,
      "borrower": 1,
      "due": 3628620,
      "returned": true
    }
  ]
}
```

### borrow-novice: фактическая память НПС1 после прогона

```json
{
  "knowledge": {
    "interaction": "compliment",
    "mastery": 0.6190518492396687,
    "expected_pleasure": 0.5,
    "expected_acceptance": 0.7,
    "confidence": 0.8,
    "source": 254,
    "debug_primary_response": 0.4
  },
  "item": [
    {
      "id": 2000,
      "owner": 2,
      "holder": 1,
      "owner_status": "confirmed",
      "holder_status": "confirmed",
      "origin": "observed",
      "source": 254,
      "expected_use": 0.6,
      "prestige": 0.3,
      "uses": 15,
      "claims": [
        {
          "source": 158,
          "speaker": 2,
          "owner": 2,
          "holder": 2,
          "origin": "reported",
          "at": 14673
        },
        {
          "source": 163,
          "speaker": 2,
          "owner": 2,
          "holder": 1,
          "origin": "agreement",
          "at": 28620
        },
        {
          "source": 174,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 91555
        },
        {
          "source": 184,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 154439
        },
        {
          "source": 196,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 220903
        },
        {
          "source": 203,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 287367
        },
        {
          "source": 210,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 353831
        },
        {
          "source": 213,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 416715
        },
        {
          "source": 218,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 479599
        },
        {
          "source": 223,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 542483
        },
        {
          "source": 227,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 605367
        },
        {
          "source": 232,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 668251
        },
        {
          "source": 235,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 731135
        },
        {
          "source": 240,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 794019
        },
        {
          "source": 246,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 856903
        },
        {
          "source": 251,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 923460
        },
        {
          "source": 254,
          "speaker": 0,
          "owner": 0,
          "holder": 1,
          "origin": "observed",
          "at": 986344
        }
      ]
    }
  ],
  "loan": [
    {
      "id": 160,
      "object": 2000,
      "lender": 2,
      "borrower": 1,
      "due": 3628620,
      "returned": false
    }
  ]
}
```
