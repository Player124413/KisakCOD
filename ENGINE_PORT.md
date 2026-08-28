# ENGINE_PORT.md — как KisakCOD переносится на Android

Этот документ — техническая карта порта для разработчиков: что уже сделано,
что проверено в коде движка, и как довести интеграцию до конца. Он честно
разделяет **готовое и проверенное** и **запланированное**.

## 1. Архитектура

```
┌─────────────────────────────── Android устройство ───────────────────────────────┐
│                                                                                  │
│  LauncherActivity (Kotlin)                                                       │
│    └─ SAF-импорт файлов игры -> <filesDir>/main, zone, players (GameFileImporter)│
│    └─ профиль производительности -> android_launcher.cfg (PerfProfile)           │
│                                                                                  │
│  GameActivity (Kotlin)                                                           │
│    ├─ GameSurfaceView (ANativeWindow → EGL)                                       │
│    ├─ TouchOverlay (сенсорное управление + режим EDIT)                           │
│    └─ Choreographer → nativeStep(dt) — цикл кадров с FPS-лимитом                  │
│                                                                                  │
│  libkisakcod_game.so (jni/)                                                      │
│    ├─ host.cpp         JNI: surface/step/input упаковка                          │
│    ├─ egl_host.cpp     EGL контекст + swap                                       │
│    ├─ engine_api.h     таблица указателей на функции движка                     │
│    │                    (engine_shell.cpp — заглушки / engine_real.cpp — движок) │
│    └─ and_touch.cpp    сенсорный пайплайн (общий для shell и движка)             │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘

        Движок (M2/M3), собирается тем же CMake:
        src/_platform/android/*   (платформенный слой: события, потоки, память)
        src/gfx_gles/*            (рендер: замена gfx_d3d)
        scripts/platform/android/platform.cmake (целевая форма сборки)
```

Ключевая идея: **сенсорный пайплайн и платформенный слой не зависят от движка**
(чистый C++, никаких include движка). Их можно тестировать и отлаживать без
сборки движка — что и сделано (см. `android/tests/`).

## 2. Проверенные факты о движке (результаты исследования)

Всё ниже вычитано непосредственно из кода репозитория:

| Факт | Где |
|---|---|
| Точка входа: `main() → Com_Init(sys_cmdline) → while(1) Com_Frame()` | `src/win32/win_main.cpp` |
| Ввод: `Sys_QueEvent(time, SE_KEY, keynum, down, 0, 0)` → пул → `Sys_GetEvent` → `Com_EventLoop` | `win_main.cpp`, `common.cpp:233` |
| Мышь: `CL_MouseEvent(x, y, dx, dy)` — dx/dy для look, x/y для меню | `client/cl_input.cpp:2044` |
| `IN_Frame()` вызывается движком каждый кадр | `common.cpp:270,2194` |
| Серия событий: `SE_NONE=0, SE_KEY=1, SE_CHAR=2, SE_CONSOLE=3` | `qcommon/qcommon.h:14` |
| Клавиши: `K_TAB=9, K_ENTER=13, K_ESCAPE=27, K_SPACE=32, K_CTRL=159, K_SHIFT=160, K_MOUSE1=200, K_MOUSE2=201` | `ui/keycodes.h` |
| Базовый путь: `fs_basepath` (dvar) ← `Sys_Cwd()` при регистрации | `universal/com_files.cpp:1309` |
| Память: `VirtualAlloc` в ханке/зоне | `universal/com_memory.cpp` |
| Потоки: `CreateThread` в `Sys_CreateThread` | `qcommon/threads.cpp:174` |
| Звук: `KISAK_OPENAL=ON` переключает Miles/MSS → OpenAL | `CMakeLists.txt`, `scripts/mp/CMakeLists.txt` |
| Интерфейс: рисуется через `R_AddCmdDrawQuadPic/…DrawText…` + материалы | `ui_mp/ui_main_mp.cpp` |
| Публичная поверхность рендера: **1433** символа `R_*`/`RB_*` из не-gfx кода | grep по `src/` |
| Объём D3D9-бэкенда: **181 файл / ~97 000 строк** (`gfx_d3d`) | — |

## 3. Слой платформы (M2) — состояние

`src/_platform/android/`:

| Файл | Что внутри | Статус |
|---|---|---|
| `and_touch.h/.cpp` | сенсор → клавиши/мышь; deadzone, кривые, инверсия, мастер-выключатель, курсор меню | ✅ готово + тесты |
| `and_sys.h/.cpp` | время (monotonic), data-dir, лог, sleep, `VirtualAlloc/Free/Protect`, потоки | ✅ готово + тесты |
| `and_main.cpp` | очередь событий `Sys_QueEvent/Sys_GetEvent`, `IN_Frame`, хуки движка, `Sys_Init` | 🚧 написано, компилируется только в связке с движком (M2) |

Что осталось для полной сборки `KisakCOD-dedi` под Android:

1. **Дружелюбность заголовков к NDK:** `qcommon/qcommon.h` включает
   `<xmmintrin.h>/<intrin.h>`; `win_local.h` — `winsock.h/dinput.h`. Для ARM
   нужны `#if defined(_WIN32)`-обёртки. Внимание: `qcommon/common.cpp`
   напрямую включает `../win32/win_local.h`, `<win32/win_net.h>`,
   `<win32/win_storage.h>` и `<gfx_d3d/r_init.h|r_rendercmds.h|r_scene.h>`,
   поэтому первым шагом M2 эти заголовки переводятся в переносимый вид
   (win_local/win_net/win_storage → `_platform/android`-двойники, gfx_d3d →
   `gfx_gles`-двойники, D3D-типы остаются под `#if defined(_WIN32)`).
2. **Сеть:** win32-слой UDP на Winsock → BSD-сокеты (или шим
   `ioctlsocket/WSAGetLastError` поверх POSIX) — `win_net.cpp` аналог.
3. **Файловая система:** движок использует `_findfirst/_findnext` (MSVC) —
   Android-слой FS-перечисления (реализовано в `and_sys` расширении).
4. **Сборка:** подключить `scripts/platform/android/platform.cmake`,
   исключить `WIN32_SRC`, собрать `KisakCOD-dedi` с `ANDROID_GLUE_SRC`.

## 4. Рендер gfx_gles (M3) — состояние

`src/gfx_gles/`:

| Файл | Статус |
|---|---|
| `r_image_dds.h/.cpp` | DDS/DXT1/DXT3/DXT5 + RGBA/BGRA/L8/A8 → RGBA8 — ✅ готово + тесты |
| `gl_renderer.h/.cpp` | каркас GLES-бэкенда: `R_Init/BeginFrame/EndFrame/RenderScene` (boot+menu-срез) | 🚧 M3 |

План перевода D3D9 → GLES3 (по слоям снизу вверх):

1. **Заголовки-двойники** `gfx_gles/r_*.h` — зеркалят публичные заголовки
   `gfx_d3d/r_*.h` (структуры `GfxPackedVertex`, `srfTriangles_t`,
   `GfxVertexBufferState` — уже без `IDirect3D*`), чтобы клиент/UI/кgame
   компилировались без правок.
2. **Ресурсы:** текстуры (DDS готов), материалы (Material + техники), шрифты —
   битовые карты из IWD; вершинные/индексные буферы → VBO/IBO.
3. **Командный конвейер:** выполнение `R_AddCmd*` 2D-списка отрисовки
   (меню, HUD, консоль) — первый видимый результат.
4. **Сцена:** DPVS/поверхности → шейдеры-техники GLES3 (минимальный набор для
   M3: colormap/lit/specular; пост-эффекты — позже).
5. **Шины с хостом:** размер вьюпорта = `render_scale` (динамическое
   разрешение для слабых SoC, уже заложено в настройки лаунчера).

Публичная поверхность — 1433 символа; бэкенд пишется итеративно, каждая итерация
заканчивается компилирующимся `libkisakcod_game.so`.

## 5. Планируемые milestone-ы

| Milestone | Содержание | Критерий готовности |
|---|---|---|
| **M1** | лаунчер, импорт, оверлей+EDIT, профили, JNI/EGL-хост, shell-режим | ✅ тесты зелёные; APK собирается |
| **M2** | платформенный слой в связке с движком; `KisakCOD-dedi` для Android; OpenAL | загружается головной сервер на устройстве |
| **M3** | gfx_gles: меню/HUD/консоль на GLES3; вход в матч с ИИ-бота | играбельное меню + загрузка карты |
| **M4** | 3D-сцена, свет, оптимизации (динамическое разрешение, многопоток), геймпад | полноценная игра на телефоне |

## 6. Известные ограничения M1

- shell-режим: без движка рендерится демо-сцена (управление/оверлей видно).
- в движковом режиме пока не подключены: сеть, звук, полный рендер (M2/M3).
- `uiCursorMode` (курсор в меню) — заглушка false до M3 (look-джойстик шлёт
  дельты; для меню будет абсолютный курсор когда UI подтвердит catcher).

## 7. Как контрибьютить

1. Нативные тесты: `bash android/tests/build_and_run.sh` — все зелёные.
2. Любая новая логика на C++ должна компилироваться чистым `g++ -fsyntax-only`
   (шимы в `jni/gles_stub.h`/`jni/jni_shim.h` это позволяют).
3. Kotlin-часть без зависимостей (`android.*` только) — сборка офлайн.
4. Для M2/M3 правьте точечно заголовки движка `#if defined(_WIN32)`,
   помечайте `KISAKTODO-ANDROID` и ведите статус в этом файле.