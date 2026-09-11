# Навигация по репозиторию

Статусы: `normative` — обязательный материал модели; `implemented` —
реализованный путь; `planned` — направление развития; `historical` — контекст,
который не определяет текущее поведение.

| Нужно найти | Точка входа | Статус |
| --- | --- | --- |
| Механики BEHAVIOR-0.3 | `docs/rules/behavior_0.3/README.md` | normative |
| Редакция и порядок версий | `docs/rules/behavior_0.3/Редакция_поведения_v0.3.md` | normative |
| Лабораторный профиль | `game/profiles/behavior_0.3/` | normative |
| Контракт C++ 0.6 | `docs/architecture/cpp_0.6/CPP_0.6.md` | implemented |
| C++ API и ядро | `engine/include/mld/`, затем `engine/src/` | implemented |
| Генерация и автономия | `engine/src/simulation/` и `engine/src/autonomy/` | implemented |
| CLI симулятора | `apps/simulate/simulate.cpp` | implemented |
| Тесты C++ | `engine/tests/` | implemented |
| Численные проверки механик | `docs/verification/behavior_0.3/` | implemented |
| Архивные результаты 0.6 | `docs/reports/cpp_0.6/` | historical |
| WORLD-0.5.1-t1 | `legacy/world_0.5.1-t1/` и `docs/history/world_0.5.1-t1/` | historical |
| Происхождение поставок | `provenance/` | historical |

Машиночитаемые связи находятся в `code-map.json` и `doc-map.json`. Перед
изменением структуры запустите `python tools/verify_navigation.py`.
