# My Life Deviation — C++ 0.8.0 / COG-0.4

Исследовательский мир с интегрированными последовательными когнитивными операциями. Без LLM и без зависимости от UE5. Мысли занимают игровое время, используют личное представление, сохраняются и влияют на реально исполняемые действия.

**Это не полная реализация всей COG-0.4.** [Матрица реализации и ограничений](docs/IMPLEMENTATION_STATUS.md) описывает, что подключено и что осталось приближением. [Результаты и ошибки](reports/cog08/RESULTS.md); [примеры настоящих мыслей](reports/cog08/THOUGHT_EXAMPLES.md).

## Сборка

CMake3.20+, C++20. Только стандартная библиотека C++; скрипт проверки JSONL использует Python3.10+. Linux GCC/Clang проверены. Windows/MSVC не проверялись.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure --no-tests=error
```

## Симуляция и мысли

```sh
./build/life_sim --population 16 --seed 42 --days 1 --summary day.json --actors actors.csv --thoughts thoughts.jsonl --trace actions.jsonl --trace-actor 1
python tools/analyze_thoughts.py thoughts.jsonl --out thought-audit.json
./build/life_sim --population 16 --days 7 --summary week.json
./build/life_sim --population 128 --seconds 21600 --summary population128.json
./build/life_sim --population 16 --days 2 --scenario scarcity --summary scarcity.json
./build/life_sim --population 8 --days 2 --scenario closed-road --summary blocked.json
./build/cognitive_scenarios scenario-results
```

`--trace-actor 1` фильтрует оба журнала. Без него сохраняются все НПС; длительный полный JSONL может быть большим. `cognitive_scenarios` задаёт авторские начальные условия: одинаковый отказ с разными личными ожиданиями, бедный голодный НПС с нормой против присвоения, разная скорость обработки. Последующие мысли исполняются обычным World, не выводятся готовым рассказом.

## Сохранение и точные режимы

```sh
./build/life_sim --population 16 --seconds 777 --save mid.save
./build/life_sim --load mid.save --seconds 600 --summary resumed.json
./build/life_sim --population 32 --seconds 3600 --reference --summary reference.json
./build/life_sim --population 32 --seconds 3600 --workers 4 --work-chunk 2 --summary workers.json
```

Сохранения 0.7 несовместимы: нет молчаливого заполнения мыслей. Полные хеши сравнивать при одинаковых seed/профиле/длительности, в одной сборке. Wall-time и поисковые счётчики могут различаться. Пул распараллеливает только чистые прогнозы; для этого небольшого каталога четыре потока оказались медленнее одного. По умолчанию один.

Для санитайзеров: `-DLIFE_SANITIZERS=ON -DCOG_SANITIZE=ON`. Полный Release-набор прошёл; под ASan/UBSan подтверждён поднабор без длительной группы world. См. отчёт, не приравнивать эти результаты.

## Структура выдачи

`SOURCE_BASE.json` фиксирует входные архивы. `reports/cog08` содержит новые исходные результаты и тесты. Остальные файлы в `reports/` относятся к исторической 0.7 и не являются подтверждением 0.8. `src/persistence.cpp` — исторический неиспользуемый фрагмент; собираются явно перечисленные файлы `src/life`.

Patch выдачи рассчитан на предоставленный архив 0.7.0, не на произвольное состояние GitHub main. Этот комплект не является подтверждением публикации нового кода в GitHub.
