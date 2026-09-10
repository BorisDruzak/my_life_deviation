# T0 выполнен: исправления текущей политики NPC

> Историческая контрольная точка T0. Текущий результат: [T1](START_HERE_T1.md).

Это рабочий исходный проект с исправленным `src/controller.cpp`, а не только документация.
Общий генератор целей, новая отчётность и автономная инициатива пока остаются проектом.

## Результат

Ожидание ответа/передачи больше не блокирует household. Доступная еда и сон имеют приоритет
над inbox/proposals и явными задачами acquire/repair. Прерванная или завершённая собственная
A15 не выбирается повторно для той же версии соглашения; временный rejected не запрещает
будущую готовность. Контроллер не получает доступа к скрытому состоянию.

[Отчёт T0](reports/implementation/T0/VERIFICATION.md) содержит RED/GREEN, реальные журналы,
границы изменений и команды проверки. [STATUS.md](docs/implementation/STATUS.md) фиксирует
выполненный T0 и следующий T1. [Исходный план](docs/design/autonomy_2026-09-10/07_CODEX_PLAN.md)
сохранён, пункты T0 отмечены выполненными.

## Проверка из корня проекта

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/npc_tests --case C10_pending_response_eats_before_critical_threshold
./build/npc_tests --case C21_interrupted_joint_is_not_replayed
python3 tests/autonomy_cli_test.py build/world_sim . --out out/t0_verify
```

Последняя команда требует отсутствующий или пустой каталог и сохраняет исходные события,
решения, снимки и машинный `results.json`. Без `--out` отчёты теста временные. Для Windows
укажите путь к executable выбранной конфигурации, например `build/Release/world_sim.exe`.
Windows-сборка в этом этапе не проверялась.

Снимки и закон мира по-прежнему WORLD-0.5.0; это исправление политики, не переход на WORLD-0.6.
Исторические архивные manifest/checksum-файлы T0 не являются источником истины для Git. Текущий репозиторный срез описан `REPOSITORY_MANIFEST.json`, `SHA256SUMS` и `docs/implementation/STATUS.md`.
