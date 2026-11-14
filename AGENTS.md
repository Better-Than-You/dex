# Dex — Agent Context & Coding Rules

**Dex** is a local, offline, deterministic voice command launcher for Arch Linux / Hyprland.
- **Stack:** C++20, Qt6 (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Network`, `Test`), `layer-shell-qt`, `toml++`, `whisper.cpp` (Phase 2).
- **Design System:** Material Design 3 (M3) dark tonal palette, pill geometry, subtle elevation, clean spring motion.
- **Hard Constraint:** **NO LLMs** anywhere in the pipeline. All intent parsing, math normalization, and command routing are 100% deterministic.

## Current Phase: Phase 1 (UI + Deterministic Engine + Stubs)
Do **NOT** integrate `whisper.cpp` or live microphone capture yet.
- Audio/ASR must sit behind an abstract C++ interface (`ITranscriptProvider`).
- Use `StubTranscriptProvider` driven by Unix domain socket (`/run/user/<uid>/dex.sock`) and `QTest` suites.

## Build & Test Commands
- Configure: `cmake -B build -G Ninja -DBUILD_TESTING=ON`
- Build: `cmake --build build`
- Test: `ctest --test-dir build --output-on-failure`

## Architecture & Module Boundaries
1. `src/core/` — `Config` (`config.toml` via `toml++`), `AppController` (daemon vs. one-shot state machine), `IpcServer` (`QLocalServer`).
2. `src/engine/` — `CommandEngine`:
   - `TextNormalizer`: lowercase, strip punctuation, spoken-math canonicalization (`"into"` -> `*`, `"l.n.e"` -> `ln(e)`).
   - `MathEvaluator`: recursive-descent deterministic expression parser (`+`, `-`, `*`, `/`, `^`, `%`, `ln`, `log`, `sin`, `cos`, `tan`, `sqrt`, `pi`, `e`).
   - `RuleRouter`: regex/prefix matcher (`QRegularExpression`) loading rules from `commands.toml`.
   - `ActionDispatcher`: `QProcess::startDetached` (`$BROWSER`, app launch, shell exec) + clipboard copy (`wl-copy`).
3. `src/provider/` — `ITranscriptProvider` + `StubTranscriptProvider`.
4. `qml/` — Floating M3 pill window (`LayerShellQt`), custom M3 token singleton (`Theme.qml`), status pill, math card, confirmation bar.

## Coding Standards
- **C++20:** Use `std::optional`, `std::string_view`, `Q_OBJECT` / `QML_ELEMENT` macros (`qt_add_qml_module`). Avoid raw owning pointers.
- **QML:** Strict typing (`property string`, `required property`), zero business logic in QML—bind directly to C++ `Q_PROPERTY` / `Q_INVOKABLE` models.
- **Security:** Always percent-encode (`QUrl::toPercentEncoding`) URL query slots. Never pass raw user speech to `/bin/sh -c` unless explicitly configured in `commands.toml` with `shell = true`.
- **Context Discipline:** Do NOT preemptively read every doc. Load only the specific spec in `docs/` needed for your current task:
  - @docs/ARCHITECTURE.md
  - @docs/SPEC_ENGINE.md
  - @docs/UI_M3_SPEC.md
  - @docs/TESTING.md
