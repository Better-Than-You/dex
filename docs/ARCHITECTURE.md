# Dex System Architecture

## 1. Runtime Modes & Resource Budget
Configured in `~/.config/dex/config.toml`:
- **Default ASR Profile (`balanced`):** Targets `ggml-base.en` (~142 MB) or `ggml-small.en-q5_1` (~190 MB) in Phase 2 (~390–550 MB RAM, 4 threads). Switchable via `asr.model_profile = "light" | "balanced" | "accurate"`.
- **Lifecycle Mode (`daemon = true` default):**
  - `daemon = true`: Runs persistently in background, keeps ASR model warm in RAM, hides QML overlay until triggered (`SIGUSR1` or IPC `"SHOW"` / `"TOGGLE"`).
  - `daemon = false`: One-shot mode—opens overlay immediately on launch, processes one utterance (or manual close via `Esc`), and exits (`QCoreApplication::quit()`).

## 2. Data Flow (Phase 1 & Phase 2 Compatible)
1. `ITranscriptProvider` (`StubTranscriptProvider` in Phase 1; `AudioCapture + WhisperWorker` in Phase 2) emits `partialTextChanged(QString)`, `utteranceFinalized(QString)`, and `stateChanged(State)`.
2. `CommandEngine` receives `utteranceFinalized`:
   - `TextNormalizer` -> canonical lowercase string.
   - `MathEvaluator` -> if valid math expression, emits `mathEvaluated(expr, result)` and copies result via `wl-copy`.
   - `RuleRouter` -> matches first regex in `commands.toml`.
   - `ConfirmationGate` -> auto-executes if `confirm == false`, else transitions to `Confirming`.
   - `ActionDispatcher` -> launches detached process (`QProcess::startDetached`).
3. QML `LayerShellQt` overlay binds to `AppController` properties and renders state transitions.

## 3. IPC Control & Debug Socket (`/run/user/$UID/dex.sock`)
Using `QLocalServer` (`Qt6::Network`) handles both daemon lifecycle control and low-overhead Phase 1 stub testing:
- `SHOW` / `HIDE` / `TOGGLE` — Overlay visibility in daemon mode
- `PARTIAL:<text>` — Simulated live streaming partial transcript
- `FINAL:<text>` — Simulated finalized utterance sent to `CommandEngine`
- `CONFIRM` / `CANCEL` — Confirm or abort pending `confirm = true` action
- `QUIT` — Clean daemon shutdown

## 4. Directory Layout
- `src/main.cpp`
- `src/core/`: `Config.hpp/.cpp`, `IpcServer.hpp/.cpp`, `AppController.hpp/.cpp`
- `src/provider/`: `ITranscriptProvider.hpp`, `StubTranscriptProvider.hpp/.cpp`
- `src/engine/`: `TextNormalizer.hpp/.cpp`, `MathEvaluator.hpp/.cpp`, `RuleRouter.hpp/.cpp`, `ActionDispatcher.hpp/.cpp`
- `qml/`: `Main.qml`, `Theme.qml`, `components/StatusPill.qml`, `components/MathResultCard.qml`, `components/ConfirmBar.qml`
- `tests/`: `tst_normalizer.cpp`, `tst_math.cpp`, `tst_router.cpp`

## 5. CMake Requirements
- `find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Test)`
- `find_package(LayerShellQt REQUIRED)`
- Link `tomlplusplus` (`toml++`).
- Register QML types via `qt_add_qml_module(dex URI Dex VERSION 1.0 QML_FILES ...)`.
