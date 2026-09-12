# SELF-0.1 / 0.12.0-self01 — проверенная поставка

`source/` — полный проект C++20. База — реальный архив 0.11 experimental1, не реорганизованный GitHub main. Удалённый репозиторий не изменён. Не используйте сообщения о несуществующем SELF12 как описание состава этой поставки.

`DELIVERY.json` содержит локальный commit, источник и SHA-256, результаты проверки патча и извлечённого комплекта. `SOURCE_MANIFEST.json` — хэши исходников. Проверка после извлечения выполнена на всех 24 CTest; финальная матрица — шесть недельных миров одного бинарника. Старые диагностические этапы помечены отдельно; не смешивайте их результаты с `release_final_*`.

## Сборка и все проверки (проверено Linux/GCC)

```sh
cmake -S source -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --parallel 2
./build/life_tests
./build/self_behavior_scenario
```

## Дефицитная недельная симуляция

```sh
mkdir -p run42
./build/life_sim --adaptive-life --population 16 --seed 42 --days 7 \
  --summary run42/summary.json --self-state run42/self.json \
  --career-state run42/career.json --community-state run42/community.json \
  --life-state run42/life.json
```

Повторите seed 7 и 101. Для парного контроля добавьте `--no-self-effects`: это отключает влияние предсказаний SelfModel на последующий выбор/аффект, не само накопление интерпретированного опыта.

Полные мысли, события, изменения убеждений и сведения о вакансиях можно экспортировать флагами из `source/README.md`. Подробные журналы значительно увеличивают размер результатов.

## Применение патча

В чистом каталоге проекта из `my_life_deviation_cpp_0.11.0_experimental1.zip`:

```sh
git apply --check /path/to/SELF01_from_0.11.patch
git apply /path/to/SELF01_from_0.11.patch
```

Применение патча к реальному исходному архиву и побайтовое совпадение всех итоговых файлов проверены. Старые save несовместимы с новым форматом; не переименовывайте их заголовки.

## Содержательная оценка

Смотрите `REPORT_RU.md`, `COVERAGE_RU.md`, `evidence/FINAL_RESULTS.json` и `evidence/CAUSAL_INFORMATION_RESULT.json`. Нулевые критические интервалы в трёх проверенных мирах не означают гарантии выживания всегда. Оставшиеся обычные переоценки и ~3 часа Idle показаны в отчёте, а не скрыты. ASan/UBSan исключает три длинных задания; Windows/UE5 не проверялись.
