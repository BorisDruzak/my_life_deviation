# Git upload manifest

Каталог является **корнем** будущего `BorisDruzak/my_life_deviation`. Не создавайте дополнительный уровень `NPC_World_0.5/`.

## Включено

- весь `src/` и `include/`;
- `app/`, `data/`, `examples/`, `tests/`, `tools/`;
- `third_party/boost` для автономной сборки без скачивания зависимостей;
- `docs/spec/`, `docs/reference/`, T0/T1 implementation docs, autonomy design;
- исходная и актуальная `.drawio`;
- минимально необходимые verification reports и test fixtures.

## Не включено намеренно

Повторные build-логи, `events.jsonl`, `samples.csv`, `state.json`, `views/*` и другие результаты отдельных прогонов. Они не требуются для сборки/тестов.

## Рекомендуемая загрузка

```bash
git clone https://github.com/BorisDruzak/my_life_deviation.git
cd my_life_deviation
# скопировать сюда содержимое этого каталога, .git не удалять
git add -A
git status
git commit -m "release: NPC World 0.5.1-t1"
git push origin main
```

## Проверка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```
