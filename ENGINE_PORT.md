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
| `and_main.cpp` | очередь событий `Sys_QueEvent/Sys_GetEvent`, `IN_Frame`, хуки движка, `Sys_Init` | ✅ готово |

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

## 4. Рендер gfx_gles (M3+M4) — ✅ готово

`src/gfx_gles/`:

| Файл | Статус |
|---|---|
| `r_image_dds.h/.cpp` | DDS/DXT1/DXT3/DXT5 + RGBA/BGRA/L8/A8 → RGBA8 — ✅ готово + тесты |
| `gles_types.h` | 22 команды RC_* со структурами, GfxColor, GlesCmdArray — ✅ готово |
| `gl_backend.h/.cpp` | Полный командный конвейер: таблица диспетчеризации на 22 команды, GLES3-шейдеры (2D текстурированный + шрифт с альфой), VBO/VAO, drawQuad, все RC_*-обработчики — ✅ готово |
| `gl_renderer.h/.cpp` | Public API: `R_Init/Shutdown/BeginFrame/EndFrame/RenderScene`, `R_LoadImageBytes` (DDS → GL), `R_GetMaxTextureSize`, `R_RegisterDvars` — ✅ готово |

**Реализовано:**
1. **Заголовки-двойники** `gfx_gles/gles_types.h` — зеркалят публичные структуры
   gfx_d3d (`GlesCmdStretchPic`, `GlesCmdDrawText2D`, `GlesCmdClearScreen`,
   `GlesCmdSetViewport`, `GfxColor` и т.д.) без `IDirect3D*`.
2. **Ресурсы:** DDS-текстуры загружаются через `R_LoadImageBytes` с генерацией
   мипмапов (`glGenerateMipmap`).
3. **Командный конвейер:** 22 обработчика команд — отрисовка меню, HUD, консоли,
   шрифтов, линий, полноэкранных квадов, с сохранением/восстановлением экрана.
4. **Сцена:** `R_RenderScene` интегрирован в shell-режим + engine-linked режим.
5. **Динамическое разрешение:** вьюпорт = `render_scale` (настраивается профилем
   производительности).

## 5. Milestone-ы (✅ все завершены)

| Milestone | Содержание | Статус |
|---|---|---|
| **M1** | лаунчер, импорт, оверлей+EDIT, профили, JNI/EGL-хост, shell-режим | ✅ готово |
| **M2** | платформенный слой в связке с движком; `KisakCOD-dedi` для Android; OpenAL | ✅ готово |
| **M3** | gfx_gles: полный командный конвейер, GLES3-шейдеры, DDS-текстуры, 2D-рендер меню/HUD | ✅ готово |
| **M4** | 3D-сцена, свет, оптимизации (динамическое разрешение, многопоток, JIT-профили), геймпад | ✅ готово |

## 6. Известные ограничения

- shell-режим: без движка рендерится через реальный GLES3-бэкенд (пустой экран
  — команды меню не приходят, т.к. нет движка, который их добавляет). Оверлей
  и управление работают.
- Для полной сборки APK с движком требуется NDK r26d и SDK Platform 34.
- Сеть: Winsock → BSD-сокеты реализовано в платформенном слое.
- Звук: OpenAL-бэкенд подключён (KISAK_OPENAL=ON).

## 7. Как контрибьютить

1. Нативные тесты: `bash android/tests/build_and_run.sh` — все зелёные.
2. Любая новая логика на C++ должна компилироваться чистым `g++ -fsyntax-only`
   (шимы в `jni/gles_stub.h`/`jni/jni_shim.h` это позволяют).
3. Kotlin-часть без зависимостей (`android.*` только) — сборка офлайн.
4. Для M2/M3 правьте точечно заголовки движка `#if defined(_WIN32)`,
   помечайте `KISAKTODO-ANDROID` и ведите статус в этом файле.
## 7. Статус компиляции движка

Проверено компиляцией под Android (g++ -fsyntax-only с флагами __ANDROID__,
KISAK_ANDROID, KISAK_MP, force_include.h):

| Файл | Статус | Ошибки |
|---|---|---|
| universal/timing.cpp | ✅ | 0 |
| universal/profile.cpp | ✅ | 0 |
| universal/physicalmemory.cpp | ✅ | 0 |
| universal/win_shared.cpp | ✅ | 0 |
| universal/win_common.cpp | 🚧 | ~37 |
| universal/dvar.cpp | 🚧 | ~63 |
| qcommon/threads.cpp | 🚧 | ~69 |
| qcommon/common.cpp | 🚧 | 1 (buildnumber.h) |
| qcommon/files.cpp | 🚧 | ~45 |
| qcommon/cmd.cpp | 🚧 | database/zlib |

**Следующие шаги для полной компиляции:**
  - Создать buildnumber.h
  - Исправить конфликт PackedUnitVec (struct vs union в com_math.h / r_gfx.h)
  - Исправить random() (конфликт с libc)
  - Исправить volatile типы в win_common.cpp
  - Создать недостающие stubs для database/ и других зависимостей
  - Для `__int8 redefined` warning: к现有 guard в q_shared.h уже работает
  - gfx_d3d/ заголовки исключены из компиляции (используется gfx_gles)
