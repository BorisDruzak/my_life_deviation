# my_life_deviation — 0.12.0-self01

C++20-симуляция NPC без LLM. Проверяемая база — полный архив 0.11;
не отсутствующая поставка SELF12 и не reorganized GitHub main.

**Главное:** [отчёт и новые измерения](docs/reports/self_model_0.1/SELF01_REPORT_RU.md),
[покрытие всех разделов SELF-MODEL-0.1](docs/architecture/self_model_0.1/IMPLEMENTATION_AND_COVERAGE_RU.md),
[спецификация SELF-MODEL-0.1](docs/rules/self_model_0.1/SELF-MODEL-0.1.md).
Старые docs/reports 0.10/0.11 сохранены как история, не доказательство новой сборки.

## Сборка / полная проверка
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure --parallel 2
./build/life_tests
./build/self_behavior_scenario
```
Проверенная платформа: Linux GCC. Для multi-config генератора Windows путь
обычно `build/Release/life_sim.exe`, но Windows/MSVC и UE5 этой поставкой не тестировались.

## Низкие стартовые деньги, доступная работа, включённый SelfModel
```sh
./build/life_sim --adaptive-life --population 16 --seed 42 --days 7 \
  --summary summary.json --self-state self.json --career-state career.json \
  --life-state life.json --community-state community.json --save week.save
```
`--adaptive-life` включает life-projects, community, SelfModel и 4 организации.
Начальные деньги 18–36, 2 порции еды, 4 безработных из 16, суммарная ёмкость 40 мест.
Технические ошибки поведения не исправляются бесплатным начислением денег.

Контроль: та же команда с `--no-self-effects` сохраняет обработку опыта, но отключает
влияние SelfPrediction на выбор/аффект. Сравнивать нужно на одинаковом seed и профиле.
Отдельное включение SelfModel без кадрового профиля: `--life-projects --self-model`.

## Происхождение изменения решения
```sh
./build/life_sim --adaptive-life --population 16 --seed 7 --days 1 \
  --thoughts thoughts.jsonl --self-updates self_updates.jsonl \
  --trace actions.jsonl --trace-actor 4 --career-state career.json --self-state self.json
```
`--self-updates` содержит actor/source/domain/axis/before/observation/after;
thoughts связывает интерпретацию, атрибуцию, сравнение и намерение;
career хранит конкретные source и изменения полей условий работы.
Включение диагностики не изменяет решения (отдельная приёмка).

## Сохранение
```sh
./build/life_sim --load week.save --days 1 --summary next_day.json
```
Новый формат `LIFE-SAVE-0.12.0-self1-r1`. Старые сохранения явно несовместимы:
нет скрытой миграции с неверной психологической историей.

## Санитайзеры
```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DLIFE_SANITIZERS=ON
cmake --build build-asan --parallel 2
ctest --test-dir build-asan --output-on-failure --parallel 1 -E '^(world|community_long|life11_long)$'
```
Три длительных задания проверяются в Release отдельно; их исключение здесь не PASS.
Сборка/логи, SHA-256 бинарника и provenance запуска находятся в приложенном evidence.
Патч поставки рассчитан на исходный архив 0.11 и сохранён как данные; текущая
репозиторная миграция использует проверенное отображение исходных файлов,
зафиксированное в `provenance/incoming/self01_0.12/`.
