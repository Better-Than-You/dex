---
name: dex-dev
description: Architect, implement, test, and debug Dex — a deterministic C++20/Qt6/QML Material 3 voice command launcher for Arch Linux and Hyprland using layer-shell-qt, toml++, recursive-descent spoken math evaluation, sherpa-onnx ASR, Silero VAD, and Unix socket IPC without LLMs. Use whenever working on Dex C++20 modules, QML Material 3 UI, TOML command routing, spoken math parsing, live audio/ASR, or QtTest/IPC verification.
---

# Dex Development Skill

**Dex** is an offline, deterministic voice command launcher for Arch Linux / Hyprland (`C++20`, `Qt6`, `QML`, `layer-shell-qt`, `toml++`, `sherpa-onnx`, Silero VAD).
**Strict Rule:** Zero LLMs in the runtime pipeline. All text normalization, spoken math evaluation, and command dispatch are 100% deterministic.

## Current Phase: Phase 2 (Live Audio + sherpa-onnx ASR) — complete
- Live 16 kHz mono capture via Qt Multimedia, lock-free SPSC ring buffer, Silero VAD, and offline sherpa-onnx recognition on a dedicated `QThread`.
- Profiles: `"moonshine"` (default, fast) and `"parakeet"` (larger, more accurate). Hot-switch via config or `SET_MODEL:`.
- Do **not** reintroduce `whisper.cpp`; the backend is sherpa-onnx via its C API.
- Keep `StubTranscriptProvider` intact and selectable with `--stub`, `asr.provider = "stub"`, or `SET_MODEL:stub`.
- ASR stays behind `ITranscriptProvider` (`start`/`stop`/`setModelProfile`, `partialTextChanged`, `utteranceFinalized`, `stateChanged`, `levelChanged`, `errorTextChanged`).

## Build & Verify Commands
- Configure: `cmake -B build -G Ninja -DBUILD_TESTING=ON`
- Build: `cmake --build build`
- Unit tests: `ctest --test-dir build --output-on-failure` (8 suites)
- Download models: `bash scripts/download-models.sh`
- Stub smoke test: `./build/dex --stub --socket /tmp/dex-stub.sock`
- IPC smoke test: `bash .opencode/skills/dex-dev/scripts/ipc-smoke.sh`

## Privilege Escalation
- Whenever elevated privileges are needed (installing packages, writing to system paths), use `pkexec` — never `sudo`.
  Example: `pkexec pacman -S --needed --noconfirm layer-shell-qt`

## Module Layout & Boundaries
1. `src/core/` — `Config` (`toml++` loader for `config.toml`), `AppController` (daemon vs. one-shot state machine, provider lifecycle + stub fallback), `IpcServer` (`QLocalServer`).
2. `src/engine/` — `CommandEngine`:
   - `TextNormalizer`: lowercase, spoken-math canonicalization (`"into"` -> `*`, `"l.n.e"` -> `ln(e)`), decimal-safe punctuation stripping.
   - `MathEvaluator`: C++20 recursive-descent / shunting-yard parser (`+ - * / ^ %`, `ln`, `log`, `sin`, `cos`, `tan`, `sqrt`, `pi`, `e`, unary minus, implicit multiplication).
   - `RuleRouter`: top-to-bottom `QRegularExpression` matching against `commands.toml` with named slot extraction (`%{query}`, `%{app}`, `%{num}`).
   - `ActionDispatcher`: `QProcess::startDetached` (`$BROWSER` fallback `xdg-open`, app launch) + `wl-copy` clipboard integration.
3. `src/provider/` — `ITranscriptProvider`; `StubTranscriptProvider`; `AudioCapture`; `SpscRingBuffer`; `ModelLayout`; `SherpaTranscriptProvider`/`SherpaWorker`; `ProviderFactory`.
4. `qml/` — `Main.qml` (`LayerShellQt` overlay), `Theme.qml` (M3 dark tonal singleton), `components/` (`StatusPill.qml` with audio-level pulse, `MathResultCard.qml`, `ConfirmBar.qml`).

## ASR Gotchas
- Link `sherpa-onnx/c-api/c-api.h` + `libsherpa-onnx-c-api.so`. The prebuilt C++ wrapper has a pre-C++11 libstdc++ ABI and fails to link.
- Moonshine: leave `model_type` empty; use `moonshine.encoder` + `moonshine.merged_decoder`.
- Parakeet: transducer encoder/decoder/joiner with `model_type = "nemo_transducer"`.
- Never touch sherpa objects from the GUI thread. All decoding happens on `SherpaWorker`.
- Partials re-decode only a tail (Moonshine 8 s, Parakeet 3 s; Parakeet min interval 900 ms). Finals use the full VAD segment.
- At speech start, seed the active buffer with ~600 ms pre-roll to avoid clipping the first phoneme.
- If the model bundle is missing, fall back to the stub and surface the error rather than aborting.

## Coding & Security Rules
- **C++20 / Qt6:** Use `Q_OBJECT` / `QML_ELEMENT` with `qt_add_qml_module(dex URI Dex VERSION 1.0 ...)`. Prefer `std::optional`, `std::string_view`, and smart pointers.
- **QML:** Strict property types (`property string`, `required property`). Keep all parsing/routing in C++; QML only binds to `AppController` properties/signals.
- **Injection Safety:** Always percent-encode URL query slots via `QUrl::toPercentEncoding()`. Never invoke `/bin/sh -c` unless explicitly flagged in `commands.toml`.

## On-Demand Reference Files
Load only the reference file matching the active subtask to keep context minimal:
- `references/engine-math-rules.md` — TOML schemas, spoken-math regex table, parser edge cases, confirmation gate.
- `references/ui-m3-hyprland.md` — `LayerShellQt` window flags, keyboard interactivity switching, M3 color/shape tokens, QML states.
- `references/ipc-and-testing.md` — Unix socket protocol specification, `QtTest` matrix, stub and end-to-end ASR verification.
