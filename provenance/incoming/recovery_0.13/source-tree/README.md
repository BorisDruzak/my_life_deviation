# My Life Deviation — 0.13.0-recovery1

C++20-исследовательская симуляция NPC без LLM. Этот патч создан **поверх полного
архива SELF01 0.12**, а не поверх реорганизованного GitHub main. Сохраняет SelfModel,
карьеру, знакомые процедуры и параллельный разговор; добавляет проверочный профиль
одежды, ресурсов, помощи, обучения процедурам и текстового телефона.

- [Точные исполняемые правила и ограничения](docs/recovery_0_13/SPEC_RU.md)
- [План и область восстановления](docs/recovery_0_13/PLAN_RU.md)
- [Результаты новой проверки](docs/recovery_0_13/REPORT_RU.md)
- [Сохранённое покрытие SelfModel](docs/self_model/IMPLEMENTATION_AND_COVERAGE_RU.md)
- [Исходная спецификация SELF-MODEL-0.1](docs/self_model/SELF-MODEL-0.1.md)

Документы и старые отчёты 0.10/0.11/0.12 оставлены как история. Они не являются
результатами новой сборки; новые доказательства поставляются в `evidence/` архива.

## Сборка и тесты
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure --parallel 2
./build/life_tests
./build/self_behavior_scenario
```
Проверяется Linux GCC, не UE5 и не Windows/MSVC. Для multi-config генератора
исполняемые файлы обычно находятся в `build/Release/`.

## Новый дефицитный мир
```sh
./build/life_sim --recovery --population 16 --seed 42 --days 7 \
  --summary summary.json --recovery-state recovery.json \
  --self-state self.json --career-state career.json \
  --community-state community.json --actors actors.csv --save week.save
```
`--recovery` включает зависимости: community, life-projects, SelfModel,
адаптивную занятость. Низкий стартовый капитал 18–36, две порции еды,
четыре организации; на 16 NPC ёмкость 40 рабочих мест. Одежда и телефон заданы
в начальной собственности отдельно от наличных денег. Запас магазина конечен,
поставки одежды и внешняя оплата труда явно не являются закрытой экономикой.

Прежний профиль доступен как `--adaptive-life`; он не включает новые товары и
телефоны. Это не побайтовый контроль всей 0.12: общие исправления источников опыта
и критических сигналов действуют и в старых режимах. `--no-self-effects` отключает
обратное влияние SelfPrediction, а не само обучение из опыта.

## Причинная диагностика
```sh
./build/life_sim --recovery --population 16 --seed 42 --days 1 \
  --thoughts thoughts.jsonl --self-updates self_updates.jsonl \
  --trace events.jsonl --trace-actor 3 --recovery-state recovery.json
```
`recovery.json`: товарный баланс, реальные отправки/доставки/чтения, помощь,
объяснения и отдельно впервые усвоенные процедуры; у NPC — типизированные факты,
источники, вопросы, черновики и ограниченные причинные записи. Журнал разработчика
не используется как знание NPC. Подробная трасса за неделю может быть большой.

## Сохранение
```sh
./build/life_sim --load week.save --days 1 --summary next.json
```
Формат `LIFE-SAVE-0.13.0-recovery1-r1`. Старые файлы сохранения отвергаются явно;
нет скрытой миграции, теряющей психологический опыт или собственность предметов.
Исходники 0.12 остаются отдельной базой патча.

## Санитайзеры
```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DLIFE_SANITIZERS=ON
cmake --build build-asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-asan --output-on-failure --parallel 1 \
  -E '^(world|community_long|life11_long)$'
```
Три исключённых длительных задания проходят отдельно в Release. Исключение
не означает PASS под санитайзером. Команды и коды — в `evidence/*/execution.json`.

## Применение патча
Патч рассчитан на каталог `source/`, извлечённый из
`my_life_deviation_SELF01_verified_0.12.0.zip`.
```sh
git apply --check /path/to/RECOVERY_0.13_from_SELF01_0.12.patch
git apply /path/to/RECOVERY_0.13_from_SELF01_0.12.patch
```
Не применять вслепую к другой структуре GitHub main. Удалённая ветка этим выпуском
не изменяется. Проверка применения и совпадения файлов включается в DELIVERY.json.
