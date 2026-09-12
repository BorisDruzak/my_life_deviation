# Связь игры с ядром

```text
BEHAVIOR-0.3 ─► COG-0.4 ─► COMMUNITY-0.10 + игровые профили
                                           │
                                           ▼
apps/simulate/life_main.cpp ─► engine/include/life (публичный API)
                                           │
                                           ▼
engine/src/life: world ─► cognition ─► social ─► community
                                           │
                                           └──► CTest, анализаторы и historical reports
```

`life_sim --community` создаёт профиль поселения. Физическое продвижение,
субъективные вычисления, социальные операции и community-механики остаются
отдельными исходными модулями; COG reference проверяет локальные контракты, но
не является вторым исполнителем мира. `--reference` отключает пространственное
ускорение, не меняя модельную скорость мыслей.

BEHAVIOR-проверки и community-анализаторы проверяют документные и отчётные
контракты. Они не загружают архивные reports как модельные входы.

C++ 0.6 и WORLD-0.5.1-t1 находятся только в historical-слоях и не строятся
root CMake.
