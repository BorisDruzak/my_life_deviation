# API и форматы WORLD-0.5

## Минимальное использование C++

```cpp
#include "npc/world.hpp"

int main() {
    auto config = npc::Config::load("data");
    auto scenario = npc::read_json("examples/food.json");
    auto world = npc::World::from_scenario(config, npc::at(scenario, "world"));
    world.run(10); // время идёт; без контроллера новые команды не возникают
    world.check_invariants();
    npc::write_json("out/api/state.json", world.snapshot());
}
```

Пример в `app/api_example.cpp` собирается отдельной целью, чтобы не оставлять непроверяемый фрагмент инструкции.

```cpp
npc::Command c;
c.actor = "a";
c.sequence = 1;
c.type = "A01";
c.args = npc::Object{{"minutes", 15}};
c.intent = "example_wait";
auto receipt = world.submit(c);  // queued, без физического эффекта
world.advance();               // начало и одна минута
```

`submit` может бросить InputError для неправильного envelope, типа, повторного конфликтующего ID. Исполнимость конкретного действия проверяется на границе и отражается в Receipt. У `receipt`, возвращённого submit, нет автоматического обновления после выполнения: получите актуальный собственный receipt из `view` или административно из State.

## Основные методы

| Метод | Назначение |
|---|---|
| `World::from_scenario(Config, Json world)` | Начальные defaults, проверка конфигурации и мира |
| `submit(Command)` | Идемпотентная постановка команды на текущую границу |
| `advance()` | Одна игровая минута, TickReport |
| `run(minutes)` | Последовательные шаги без нового внешнего контроллера |
| `can_decide(actor)` | Только собственная способность/занятость; дешёвый запрос scheduler |
| `view(actor)` | Локальная проекция, недоступные сообщения не раскрываются |
| `hypothesize(actor, Evidence)` | Добавление собственной гипотезы со знакомыми корнями |
| `snapshot()` / `restore(Config, Json)` | Полный checkpoint только в ready |
| `check_invariants()` | Дорогая полная проверка согласованности |
| `debug_state()` | Всеведущий const-доступ для тестов/инструментов, не политика NPC |

Один World выполняется одним потоком. Параллельные эксперименты допустимы в отдельных экземплярах. Потокобезопасное совместное изменение одного экземпляра не реализовано.

## Формат сценария CLI

```json
{
  "world": {
    "time": 0,
    "profile": "baseline",
    "seed": "experiment-1",
    "places": {"room": {"id": "room"}},
    "accounts": {"a": 100},
    "actors": {
      "a": {"id": "a", "account": "a", "position": {"place": "room"},
            "needs": {"N01": 60, "N02": 70}}
    }
  },
  "controllers": {"a": {"mode": "household"}},
  "script": [
    {"at": 0, "command": {"actor": "a", "sequence": 1, "type": "A01",
                           "args": {"minutes": 10}}}
  ]
}
```

`id` каждой карты совпадает с ключом. В actor ID запрещены `@` и `/`; для переносимых файлов сценария используйте обычные ASCII-ID `[a-z0-9_-]`, русское отображаемое имя храните в `name`. Полные типы и defaults опубликованы в `model.hpp`. Actor.needs и traits допускают частичные patches; остальные вложенные структуры при patch заменяются целиком.

Скрипт имеет приоритет над автономной обычной командой для того же NPC/границы. Номер sequence не должен конфликтовать с уже использованным. Демонстрации не смешивают произвольные заранее большие последовательности и policy без согласования.

`validate` проверяет исходный мир; он не доказывает, что каждая будущая команда скрипта будет исполнима. Конфликт ресурсов или отказ другого NPC — нормальный результат симуляции, а не ошибка валидатора.

## Команда

Поля: actor, sequence>0, type=A01..A32, args, expected_versions (необязательно), intent (необязательно). Неизвестные верхнеуровневые поля отвергаются строгим decoder; параметры действия проверяются его whitelist. Результаты: queued, started, completed, rejected, interrupted.

Причина отклонения в локальной проекции намеренно общая `conditions_not_met`. Полная техническая причина находится в debug_reason/ledger. Это предотвращает запрос неизвестного ID как всеведущий поиск объектов.

## Предложения

Поддержанные `terms.kind`: loan_item, loan_money, joint, private, repair, reschedule.

```json
{"kind":"loan_item","lender":"a","borrower":"b","item":"tool","due":180}
```

```json
{"kind":"joint","place":"park","start":30,"end":90,"minutes":20,
 "topic":"conversation","attention":{"a":1,"b":0.5}}
```

Числа `start/end/due/expires` — абсолютные игровые минуты, а окна места/работы — минуты суток. Возврат по займу привязан к obligation ID; повторная передача не создаёт новый первоначальный заём.

## Сведения

```json
{
  "id":"known-tool",
  "kind":"observation",
  "source":"genesis",
  "learned_at":0,
  "roots":["initial-observation-1"],
  "confidence":0.9,
  "prior":0.5,
  "half_life":1440,
  "statement":{"subject":"tool","predicate":"item_seen",
               "arguments":{"type":"I05","holder":"a","place":"workshop"},
               "polarity":true,"valid_from":0,"valid_until":-1}
}
```

Память может содержать ошибки. Совпадение структуры предмета в записи не даёт доступ к реальному экземпляру; доступ проверяется при исполнении. `valid_until=-1` означает отсутствие явно заданного конца, не бесконечную уверенность.

## Административные внешние воздействия

В `world.external`: id, at, priority, issuer, kind, args, applied=false. Поддержаны injury, cash, edge, move_item, shipment. Они считаются заранее зарегистрированным тестовым/авторским вводом, а не способом NPC писать любые значения State. Их идентификаторы уникальны, аргументы и ссылки проверяются.

Поставка продовольствия создаёт новый явно названный экземпляр с provenance. При исчерпании магазина тайного автопополнения нет. Отрицательное внешнее изменение денег не должно превышать баланс; некорректный динамический внешний эффект останавливает прогон, не маскируется.

## История и повтор

Журнал содержит наблюдения и аудит, но не является полным event-sourcing набором committed deltas. Надёжное восстановление — из полного snapshot. Повтор эксперимента — исходный мир, неизменные config/seed, команды/политики и версия executable.

Снимок не включает исходный текст будущего скрипта CLI. Для resume с продолжением его необходимо передать тем же `--scenario`. Очередь уже поставленных команд, напротив, входит в snapshot.

## Расширение T1: локальные знания и память (реализовано)

`World::view_delta(actor, cursor, limit=64, receipt_ids={})` выдаёт собственный снимок тела,
не более 64 суммарных evidence/envelope по двум курсорам и до четырёх явно запрошенных
собственных receipts. `policy::apply_delta` формирует immutable LocalView и продвигает память.
`policy::query_belief` возвращает KnownTrue/KnownFalse/Unknown с временной применимостью
и источниками. `ControllerMemory::snapshot/restore` сохраняет GC-11 отдельно от WORLD.

Детальный действующий контракт, ограничения размера, temporal-семантика, валидация и пример:
[Local policy T1](../implementation/T1_LOCAL_POLICY_STATE.md). Старый полный `view` и
форматы CLI не менялись. Типы Goal/Plan ещё не выполняются как новая автономная политика.
