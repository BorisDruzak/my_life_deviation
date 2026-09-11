# Проверка реорганизации репозитория — 2026-09-10

## Подтверждённые поставки

| Архив | Назначение | Проверка внутреннего `SHA256SUMS` |
| --- | --- | --- |
| `my_life_deviation_git_ready_0.5.1-t1.zip` | каноническая реализация WORLD-0.5.1-t1 | 719 из 719 |
| `my_life_deviation_REPO_READY.zip` | историческая документация pre-T1 | 710 из 710 |

Полные SHA-256 архивов записаны в `provenance/archive-sha256.json`. C++-код
pre-T1 не переносился: из второго архива сохранены только уникальные материалы
в `docs/history/pre_t1/` и `docs/history/math_v0.3/`.

## Проверки структуры

- Нет прежних корневых каталогов `app`, `data`, `examples`, `include`, `tests`,
  `src`, `reports` и `Документация`.
- Присутствуют 16 контрольных путей слоёв `engine`, `apps`, `game`, `docs` и
  `navigation`.
- Все 33 ссылки на исходники и Python-скрипты в `CMakeLists.txt` существуют.
- Разобраны 42 JSON-файла репозитория.

## Проверки механизма навигации

```text
python -m unittest tools.tests.test_verify_navigation -v
# Ran 2 tests ... OK

python tools/verify_navigation.py
# Navigation maps valid
```

Валидатор проверяет существование путей, уникальность ID, статус и связи
`depends_on` в `navigation/code-map.json` и `navigation/doc-map.json`.

## Остаточное ограничение

На рабочей машине на момент проверки не обнаружены `cmake`, `cl`, `g++` или
`clang++`. Поэтому свежие CMake/CTest и sanitizer-прогоны не выполнялись.
Содержимое поставок и статические пути проверены, но это не заменяет нативную
сборку в окружении с C++20-инструментами.
