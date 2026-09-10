# Связь игры с ядром

```text
game/rules + game/scenarios
            │
            ▼
apps/world_sim ──► engine/include/npc (публичный API)
            │
            ▼
engine/src/core ─► simulation / actions / social / knowledge / autonomy
            │                                      │
            └────────── проверки инвариантов ◄─────┘
```

`game/rules` задаёт нормативные параметры, каталог и карту приёмки.
`game/scenarios` — входные игровые ситуации, а не исходный код механик.
`apps/world_sim` загружает их через CLI, создаёт `World` и формирует журнал
результатов. `engine` остаётся независимым от конкретного сценария.

Граница информации важна: контроллер в `engine/src/autonomy` принимает
решение только из `World::view(actor)`. Полный отладочный журнал не является
данными игрока; это подтверждено в `apps/world_sim/main.cpp` и документации
API.

Детали правил: `docs/rules/`. Реализованные механики:
`docs/game/mechanics/CURRENT_MECHANICS_T1.md`. Отчёты не переопределяют
правила — они фиксируют результаты прежних прогонов.
