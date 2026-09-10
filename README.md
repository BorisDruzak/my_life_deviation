# My Life Deviation

Репозиторий содержит C++20-ядро симуляции NPC, правила мира, игровые сценарии
и документированную историю их развития.

Начните с [навигации](navigation/INDEX.md): там разделены нормативные правила,
реализованные механики, планы и исторические материалы. Связь игры и ядра
показана в [GAME_FLOW.md](navigation/GAME_FLOW.md).

## Быстрый запуск

После установки CMake и C++20-компилятора:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
.\build\Release\world_sim.exe validate --scenario game/scenarios/household.json
```

При одноконфигурационном генераторе исполняемый файл находится в
`build/world_sim` (или `build/world_sim.exe`).

Проверка связности структуры не требует внешних библиотек:

```powershell
python tools/verify_navigation.py
python -m unittest tools.tests.test_verify_navigation
```
