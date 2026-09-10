# Навигация по репозиторию

Статусы: `normative` — обязательное правило; `implemented` — реализованное
состояние; `planned` — направление развития; `historical` — материал для
контекста, не источник текущего поведения.

| Нужно найти | Точка входа | Статус |
| --- | --- | --- |
| Правила мира | `docs/rules/WORLD_0.5.md` | normative |
| Контракты действий | `docs/rules/ACTIONS.md` | normative |
| Параметры и каталог | `game/rules/` и `docs/rules/PARAMETERS.md` | normative |
| Реализованные механики | `docs/game/mechanics/CURRENT_MECHANICS_T1.md` | implemented |
| C++ API и ядро | `engine/include/npc/`, затем `engine/src/` | implemented |
| Автономия NPC | `engine/src/autonomy/` и `docs/roadmap/autonomy_2026-09-10/` | implemented / planned |
| CLI симулятора | `apps/world_sim/main.cpp` | implemented |
| Игровые сценарии | `game/scenarios/` | implemented |
| Тесты | `engine/tests/` | implemented |
| История и прежние модели | `docs/history/` | historical |
| Происхождение поставок | `provenance/` | historical |

Машиночитаемые связи находятся в `code-map.json` и `doc-map.json`. Перед
изменением структуры запустите `python tools/verify_navigation.py`.
