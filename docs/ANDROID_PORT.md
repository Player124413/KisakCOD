# KisakCOD — Android port

Status: **work in progress**. The Android shell (launcher, device probe, touch
input, JNI bridge) exists. The engine itself does not build for Android yet.

This document records *why* the port is shaped the way it is, what is done, and
what is left. It is written to be readable by someone who has not read the rest
of the repo.

---

## 1. Why DXVK and not a GLES rewrite

The decisive fact is not the size of the renderer. It is this, from
`src/gfx_d3d/r_material.cpp`:

```cpp
dx.device->CreatePixelShader((DWORD*)loadDef->program, (IDirect3DPixelShader9 **)&mtlShader->prog);
dx.device->CreateVertexShader((DWORD*)loadDef->program, (IDirect3DVertexShader9 **)&mtlShader->prog);
```

`loadDef->program` is **pre-compiled D3D9 shader bytecode (DXSO) loaded straight
out of the game's fastfiles**. The shipped game contains no HLSL source.

So a "rewrite the renderer in GLES" port is really "write a DXSO → GLSL ES
transpiler and push every shader in the game through it". That is the expensive,
hard-to-debug part, and it is the part we skip entirely.

DXVK already contains a `dxso → SPIR-V` shader bridge, plus a complete D3D9
implementation on top of Vulkan. Android has Vulkan. Therefore:

> The renderer keeps calling plain Direct3D 9. DXVK turns it into Vulkan.
> `src/gfx_d3d/` is not rewritten.

### Measured scope

| | |
|---|---|
| `src/` total | ~640,000 LOC |
| `src/gfx_d3d/` | 97,215 LOC / 181 files |
| Files touching D3D9 directly | 41 files, ~40,000 LOC |
| Files using D3DX9 | **2**, ~10 distinct symbols |
| `src/win32/` | 7,007 LOC |
| Win32 API used outside `src/win32/` | ~80 files |
| Inline `__asm` blocks | 70 (cgame 21, win32 9, client 4, qcommon 2) |
| Files with SSE intrinsics | 9 (incl. `r_model_skin_sse.cpp`) |

The D3DX9 number matters more than it looks. DXVK implements `d3d9.dll` but
**not** `d3dx9_xx.dll`. Here D3DX9 is used only for shader constant-table
reflection and one `D3DXCompileShader` call, all confined to
`r_material_load_obj.cpp`. That is a small, well-understood shim, not a port.

### Things already in our favour

- The D3D9 code is already isolated in a backend layer (`rb_*`, `r_image`,
  `r_material`, `r_buffers`, `r_rendertarget`). The other ~57k LOC of
  `gfx_d3d` is platform-neutral scene code.
- The project already has a platform-override mechanism
  (`scripts/platform_override.cmake`): any file at
  `src/_platform/<platform>/<path>` shadows the original. `KISAK_PLATFORM`
  selects it. `scripts/platform/linux/` was already scaffolded (empty).
- Sound is already split into drivers. `src/sound/snd_driver.cpp` is guarded by
  `#ifndef KISAK_OPENAL`, and `snd_openal.cpp` / `snd_driver_openal.cpp` exist.

---

## 2. Architecture

```
┌──────────────────────────────────────────────────────────┐
│ Kotlin / Android shell                                   │
│   LauncherActivity   device probe, settings, launch      │
│   GameActivity       SurfaceView + touch overlay         │
│   TouchControlsView  all input, multi-touch              │
└───────────────┬──────────────────────────────────────────┘
                │ JNI (staging only — never calls engine)
┌───────────────▼──────────────────────────────────────────┐
│ libkisakcod.so                                           │
│   android_bridge.cpp   SPSC command ring, look accum.    │
│   game thread          PumpInput() → Com_Frame()         │
└───────────────┬──────────────────────────────────────────┘
                │
┌───────────────▼──────────────────────────────────────────┐
│ Engine — unchanged D3D9 calls                            │
└───────────────┬──────────────────────────────────────────┘
                │
┌───────────────▼──────────────────────────────────────────┐
│ DXVK (d3d9 → Vulkan, dxso → SPIR-V)                      │
└───────────────┬──────────────────────────────────────────┘
                │
┌───────────────▼──────────────────────────────────────────┐
│ Android Vulkan (Adreno / Mali / Turnip / Mesa)            │
└──────────────────────────────────────────────────────────┘
```

### Threading contract

The UI thread **never** calls into the engine. `Cbuf_AddText()` and
`CL_MouseEvent()` are only ever invoked from the game thread, inside
`Android_PumpInput()`. The UI thread writes into a lock-free SPSC ring buffer
and two atomic accumulators.

This is not defensive programming for its own sake: the engine's command buffer
is not thread-safe, and the failure mode of a race there is a corrupted command
string that shows up as a random console error minutes later.

### Input injection points (verified against the source)

| Purpose | API | Location |
|---|---|---|
| Console commands | `Cbuf_AddText(0, cmd)` | `src/qcommon/cmd.h` |
| Look (mouse delta) | `CL_MouseEvent(0, 0, dx, dy)` | `src/client/cl_input.h` |
| Key/button events | `Sys_QueEvent(time, SE_KEY, K_*, down, 0, 0)` | `src/win32/win_local.h` |

Look goes through `CL_MouseEvent` because that is the same entry point the
Win32 mouse path uses (`IN_MouseMove` → `CL_MouseEvent`), so touch look
travels exactly the code path the game already trusts. No cursor recentring
hacks.

Buttons emit **console commands**, not key codes, so a button works regardless
of the player's key binds. Every command in `GameAction.kt` was checked against
the engine's actual `Cmd_AddCommandInternal(...)` registrations in
`src/client/cl_input.cpp`.

Two gotchas found while doing that:

- **Call of Duty 4 has no jump command.** There is no `+jump` anywhere in the
  tree. That is correct, not an oversight — omitting a jump button is right.
- **`toggleads` is a plain command, not a `+`/`-` pair.** ADS is
  `toggleads` / `leaveads`.

---

## 3. What is done

| Component | File | Notes |
|---|---|---|
| Touch input engine | `android/.../input/TouchControlsView.kt` | pointer-ID tracking, floating stick, dead zone, saturation, hysteresis |
| Command model | `android/.../input/GameAction.kt` | every command verified against the engine |
| Layout model | `android/.../input/TouchLayout.kt` | normalised coords, JSON persistence |
| JNI bridge | `android/app/src/main/cpp/android_bridge.cpp` | SPSC ring, look accumulator, engine thread |
| Launcher | `android/.../LauncherActivity.kt` | device probe, settings, guarded launch |
| Game host | `android/.../GameActivity.kt` | surface lifecycle, immersive mode, back-guard |
| Device probe | `android/.../util/DeviceTier.kt` | Vulkan version, vendor, memory, tiering |
| CMake platform | `scripts/platform/android/platform.cmake` | forces OpenAL, disables Bink/Steam/Radiant |

### Touch input: the details that matter

- **Pointers are tracked by `pointerId`, never by index.** Indices are
  reassigned when a finger lifts; index-keyed state is the classic cause of
  buttons sticking down or sticks freezing.
- **Every exit path funnels through `releaseAll()`** — `ACTION_CANCEL`,
  `ACTION_UP`, `onDetachedFromWindow`, window visibility loss, `onPause`.
  A stuck `+attack` is the most damaging bug a touch layer can have, so release
  is idempotent and unconditional.
- **Digital movement has hysteresis.** The engine only accepts digital movement
  today (see §4), so the stick derives `+forward`/`+back`/`+moveleft`/`+moveright`
  with a dead zone and a hysteresis band, which stops command chatter when a
  thumb rests exactly on the threshold.
- **Hit testing is priority ordered**: buttons → sticks → look pad. Without
  this, a button drawn over the look zone would never fire.
- **Look sensitivity is resolution independent** (normalised by view size) and
  scales down while ADS is held.

---

## 4. What is left

### Stage 1 — DXVK spike (highest risk, do first)

Build DXVK for Android (meson + NDK) and run a minimal D3D9 application on an
`ANativeWindow` with a real DXSO shader extracted from a COD4 fastfile.

**Known gap:** dxvk-native's WSI layer (`src/wsi/`) ships SDL2 and GLFW
backends. An `ANativeWindow` backend for `VK_KHR_android_surface` has to be
added there. This is bounded work but it is not optional.

**Version constraint:** DXVK 2.6+ requires Vulkan 1.3. Many stock Qualcomm
Adreno drivers only advertise Vulkan 1.1. Either pin an older DXVK (1.10.x
needs only 1.1) or require Turnip/Mesa. `DeviceTier.kt` reports the device's
Vulkan version precisely so this decision can be made per device.

### Stage 2 — POSIX port of the engine (largest volume)

Target `KISAK_PLATFORM=linux` on the desktop first: it validates everything
except graphics and debugs in seconds instead of minutes.

- Replace the `Sys_*` platform contract (`src/win32/win_local.h`) for POSIX.
- ~80 files outside `src/win32/` use Win32 API directly.
- 70 inline `__asm` blocks → C or intrinsics.
- 9 files with SSE → NEON, including `r_model_skin_sse.cpp`.
- 32-bit x86 assumptions throughout (`/machine:x86` in the link line, plus
  `int`-typed pointer arithmetic inherited from the decompilation).
- Radiant (79,420 LOC) is **not** ported — it is the level editor.

### Stage 3 — Android platform layer

`src/_platform/android/` overriding: `win_main.cpp`, `win_input.cpp`,
`win_wndproc.cpp`, `win_net.cpp`, `win_storage.cpp`, `win_steam.cpp`,
`win_syscon.cpp`, `win_voice.cpp`, `win_localize.cpp`.

Plus: JNI entry, `ANativeWindow`, lifecycle, asset loading from the APK/OBB.

### Stage 4 — ARM64

SSE → NEON, 64-bit correctness pass, hunk allocator resized for mobile memory
budgets.

### Stage 5 — Performance

COD4 is CPU-heavy (CPU skinning, DPVS). On mobile this is a bigger risk than
the GPU side.

---

## 5. Dependencies

| Dep | Portable? | Plan |
|---|---|---|
| zlib | yes | builds as-is |
| Speex (group voice) | yes | builds as-is |
| dr_libs (`dr_mp3`, `dr_wav`) | yes | header-only |
| ODE physics | yes | builds as-is |
| Miles Sound System | **no** | closed 32-bit x86. Use the existing OpenAL backend (forced on in `platform.cmake`) |
| Bink | **no** | closed x86. Cinematics skipped; FFmpeg's Bink decoder is the eventual replacement |
| Steamworks | **no** | no Android SDK. Not needed for SP |
| D3D9 / D3DX9 | via DXVK | D3DX9 usage is ~10 symbols in one file; needs a small shim |

---

## 6. Building

### The Android shell (works today)

The Android shell builds and runs **today**, with the engine stubbed out. That
is deliberate: it lets the launcher and the touch layer be tested on real
hardware long before the engine port lands.

```bash
cd android
./gradlew :app:assembleDebug
```

The stub is controlled by `-DKISAK_ANDROID_STUB` in
`android/app/src/main/cpp/CMakeLists.txt` (default `ON`). In stub mode the game
thread logs queued commands and look deltas instead of running the engine, so
the input pipeline can be verified end to end.

Default game data location is `getExternalFilesDir(null)/cod4` — a real
filesystem path, no storage permission required. Copy the COD4 `main/` and
`zone/` directories there.

### The engine (stages 2–3)

The engine selects a platform through `KISAK_PLATFORM` at the top of the root
`CMakeLists.txt`:

```cmake
set(KISAK_PLATFORM android)   # was win32
```

The root `CMakeLists.txt` then automatically excludes Radiant and stops
defaulting `KISAK_OPENAL` to `OFF`; `scripts/platform/android/platform.cmake`
forces OpenAL on and declares Bink/Steam unavailable.

Switching to `android` today will fail, because the engine still contains the
Win32 platform code. That is exactly what stages 2 and 3 address.

---

## 7. Honest risk summary

| Risk | Severity | Mitigation |
|---|---|---|
| DXVK has no ANativeWindow WSI backend | high | Stage 1 spike; add it to dxvk-native's WSI layer |
| Android Vulkan drivers below DXVK's requirement | high | probe first, pin an older DXVK, or require Turnip |
| Mali / Exynos / PowerVR much less exercised with DXVK | medium | device tiering, conservative default settings |
| 32-bit x86 assumptions in 640k LOC of decompiled code | high | desktop Linux port first, where it is debuggable |
| CPU skinning + DPVS on mobile CPUs | medium | resolution scaling, LOD, frame cap |
| Bink and Miles have no ARM build | low | OpenAL already exists; cinematics skipped |
