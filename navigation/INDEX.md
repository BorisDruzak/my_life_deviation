# Навигация по репозиторию

Статусы: `normative` — обязательный материал модели; `implemented` —
реализованный путь; `planned` — направление развития; `historical` — контекст,
который не определяет текущее поведение.

| Нужно найти | Точка входа | Статус |
| --- | --- | --- |
| Механики BEHAVIOR-0.3 | `docs/rules/behavior_0.3/README.md` | normative |
| Редакция и порядок версий | `docs/rules/behavior_0.3/Редакция_поведения_v0.3.md` | normative |
| Когнитивные правила COG-0.4 | `docs/rules/cognition_0.4/README.md` | normative |
| Правила COMMUNITY-0.10 | `docs/rules/community_0.10/README.md` | normative |
| Спецификация SELF-MODEL-0.1 | `docs/rules/self_model_0.1/SELF-MODEL-0.1.md` | normative |
| Профили поведения/COG/community | `game/profiles/` | normative |
| Профиль SELF-0.1 | `game/profiles/self_model_0.1/profile.json` | normative |
| Исполняемая граница SELF-0.1 | `docs/architecture/self_model_0.1/IMPLEMENTATION_AND_COVERAGE_RU.md` | implemented |
| Граница реализации COMMUNITY-0.10 | `docs/architecture/community_0.10/README.md` | implemented |
| C++ API и ядро | `engine/include/life/`, затем `engine/src/life/` | implemented |
| COG reference | `reference/cognition_0.4/` | implemented |
| CLI симулятора | `apps/simulate/life_main.cpp` | implemented |
| Тесты C++ | `engine/tests/life/` | implemented |
| Численные проверки механик | `docs/verification/behavior_0.3/` | implemented |
| Архивные результаты COMMUNITY-0.10 | `docs/reports/community_0.10/` | historical |
| Архивные результаты SELF-0.1 | `docs/reports/self_model_0.1/` | historical |
| C++0.10 COMMUNITY core | `legacy/cpp_0.10/` | historical |
| C++ 0.6 laboratory core | `legacy/cpp_0.6/` и `docs/history/cpp_0.6/` | historical |
| WORLD-0.5.1-t1 | `legacy/world_0.5.1-t1/` и `docs/history/world_0.5.1-t1/` | historical |
| Происхождение поставок | `provenance/` | historical |

Машиночитаемые связи находятся в `code-map.json` и `doc-map.json`. Перед
изменением структуры запустите `python tools/verify_navigation.py`.
