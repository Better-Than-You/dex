# Dex — Agent Context & Coding Rules

**Dex** is a local, offline, deterministic voice command launcher for Arch Linux / Hyprland.
- **Stack:** C++20, Qt6 (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Multimedia`, `Network`, `Test`), `layer-shell-qt`, `toml++`, `sherpa-onnx` (C API) + Silero VAD.
- **Design System:** Material Design 3 (M3) dark tonal palette, pill geometry, subtle elevation, clean spring motion.
- **Hard Constraint:** **NO LLMs** anywhere in the pipeline. All intent parsing, math normalization, and command routing are 100% deterministic.

## Current Phase: Phase 2 (Live Audio + sherpa-onnx ASR)
Live capture, VAD and dual-profile ASR are implemented. Do **not** reintroduce `whisper.cpp`; the ASR backend is `sherpa-onnx`.
- All audio/ASR stays behind `ITranscriptProvider`.
- `SherpaTranscriptProvider` = `AudioCapture` → `SpscRingBuffer` → Silero VAD → sherpa-onnx offline recognizer on a dedicated `QThread`.
- `StubTranscriptProvider` must stay intact and selectable via `--stub`, `asr.provider = "stub"`, or `SET_MODEL:stub`.
- Profiles: `"moonshine"` (default, fast) and `"parakeet"` (larger, more accurate). Offline models produce partials by re-decoding a tail of the active VAD buffer.

## Build & Test Commands
- Configure: `cmake -B build -G Ninja -DBUILD_TESTING=ON`
- Build: `cmake --build build`
- Test: `ctest --test-dir build --output-on-failure`
- Models: `bash scripts/download-models.sh` (installs to `~/.local/share/dex/models`)
- Stub smoke test: `./build/dex --stub --socket /tmp/dex.sock`

**Privileges:** Whenever a sudo/elevated command is needed, use `pkexec` instead (e.g. `pkexec pacman -S --needed layer-shell-qt`).

## Architecture & Module Boundaries
1. `src/core/` — `Config` (`config.toml` via `toml++`), `AppController` (daemon vs. one-shot state machine, provider lifecycle, fallback to stub), `IpcServer` (`QLocalServer`).
2. `src/engine/` — `CommandEngine`:
   - `TextNormalizer`: lowercase, strip punctuation, spoken-math canonicalization (`"into"` -> `*`, `"l.n.e"` -> `ln(e)`).
   - `MathEvaluator`: recursive-descent deterministic expression parser (`+`, `-`, `*`, `/`, `^`, `%`, `ln`, `log`, `sin`, `cos`, `tan`, `sqrt`, `pi`, `e`).
   - `RuleRouter`: regex/prefix matcher (`QRegularExpression`) loading rules from `commands.toml`.
   - `ActionDispatcher`: `QProcess::startDetached` (`$BROWSER`, app launch, shell exec) + clipboard copy (`wl-copy`).
3. `src/provider/`:
   - `ITranscriptProvider` — contract: `start`/`stop`/`setModelProfile`, `partialTextChanged`, `utteranceFinalized`, `stateChanged`, `levelChanged`, `errorTextChanged`.
   - `StubTranscriptProvider` — IPC-driven text injection.
   - `AudioCapture` — Qt Multimedia 16 kHz mono capture + RMS level; producer side of the ring buffer.
   - `SpscRingBuffer` — lock-free single-producer/single-consumer audio queue.
   - `ModelLayout` — profile canonicalisation and model-bundle file discovery.
   - `SherpaTranscriptProvider` / `SherpaWorker` — VAD loop, partial/final decodes, model load/reload on the `dex-asr` thread.
   - `ProviderFactory` — config/flag selection and graceful fallback.
4. `qml/` — Floating M3 pill window (`LayerShellQt`), custom M3 token singleton (`Theme.qml`), status pill (with audio-level pulse), math card, confirmation bar.

## ASR Rules & Gotchas
- Link the sherpa-onnx **C API** (`sherpa-onnx/c-api/c-api.h`). The prebuilt C++ wrapper has a pre-C++11 libstdc++ ABI and will not link.
- Moonshine must leave `model_type` empty (auto-detects merged decoder v2). Parakeet requires `model_type = "nemo_transducer"`.
- Never touch sherpa-onnx objects from the GUI thread; `SherpaWorker` owns them.
- Partials decode only a tail (Moonshine 8 s, Parakeet 3 s) to stay responsive; finals use the whole VAD segment.
- If the model bundle is missing/incomplete, log the error and fall back to the stub rather than crashing.

## Coding Standards
- **C++20:** Use `std::optional`, `std::string_view`, `Q_OBJECT` / `QML_ELEMENT` macros (`qt_add_qml_module`). Avoid raw owning pointers.
- **QML:** Strict typing (`property string`, `required property`), zero business logic in QML—bind directly to C++ `Q_PROPERTY` / `Q_INVOKABLE` models.
- **Security:** Always percent-encode (`QUrl::toPercentEncoding`) URL query slots. Never pass raw user speech to `/bin/sh -c` unless explicitly configured in `commands.toml` with `shell = true`.
- **Context Discipline:** Do NOT preemptively read every doc. Load only the specific spec in `docs/` needed for your current task:
  - @docs/ARCHITECTURE.md
  - @docs/SPEC_ENGINE.md
  - @docs/UI_M3_SPEC.md
  - @docs/TESTING.md
