# T1 — локальные знания и память контроллера

T1 реализован поверх архива T0. Новый код находится в `include/npc/policy/`, `src/policy/`,
`include/npc/local_view.hpp` и `src/local_view.cpp`. Проверки — `tests/policy_goals_tests.cpp`
(35 кейсов) и `tests/policy_index_tests.cpp` (1 кейс). Ниже — действующее API, не только спецификация.

Главное: NPC получает данные порциями; непрочитанное сообщение показывает только конверт;
противоречие не разрешается последней записью; увиденный прежде товар не считается
гарантированно доступным сейчас; собственный долг восстанавливается из подписанных условий
и доступных подтверждений, а не из глобального статуса. Старые LocalView остаются неизменными.

[Состояние этапов](docs/implementation/STATUS.md) ·
[API и ограничения](docs/implementation/T1_LOCAL_POLICY_STATE.md) ·
[Проверки и RED/GREEN](reports/implementation/T1/VERIFICATION.md)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/npc_tests --case PG_T1_real_loan_partial_repayment_and_resume
```

158/158 native tests и CTest 3/3 проверены на GCC Release и Clang Debug ASan/UBSan.
Windows, недельная симуляция и 1000 NPC в T1 не проверялись.

`ControllerMemory` сохраняется отдельно от WORLD snapshot. Общая сериализация запуска,
генератор целей, выбор нового плана, отчётность и социальная инициатива ещё не реализованы.
Следующий этап — T2. Локальная ветка: `t1/local-policy-state`; GitHub не изменялся.

`SHA256SUMS` описывает текущие файлы; `T1_IMPLEMENTATION_MANIFEST.json` — изменения относительно T0.
Старые `T0_IMPLEMENTATION_MANIFEST.json`, `AUTONOMY_REVIEW_MANIFEST.json` и отчёты предыдущих этапов
сохранены как исторические документы и не являются текущим статусом T1.
