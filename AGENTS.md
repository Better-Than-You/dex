# Dex — Agent Context & Coding Rules

**Dex** is a local, offline voice command launcher for Arch Linux / Hyprland.
- **Stack:** C++20, Qt6 (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Multimedia`, `Network`, `Test`), `layer-shell-qt`, `toml++`, `sherpa-onnx` (C API) + Silero VAD, optional `llama.cpp` (C API).
- **Design System:** Material Design 3 (M3) dark tonal palette, pill geometry, subtle elevation, clean spring motion.
- **Hard Constraint:** no cloud services and no chat-style LLM in the command path. Intent parsing, math normalization, evaluation and command routing are deterministic. The optional 0.8B SLM only rewrites noisy maths/calculus speech into a JSON command, on demand.

## Current Architecture: Single-Shot Parakeet ASR + Maths/Calculus SLM
Single-shot offline ASR with an on-demand maths front-end. The ASR backend is `sherpa-onnx` using NVIDIA Parakeet.
- All audio/ASR stays behind `ITranscriptProvider`.
- `SherpaTranscriptProvider` = `AudioCapture` → `SpscRingBuffer` → Silero VAD → sherpa-onnx offline recognizer on a dedicated `QThread`.
- `StubTranscriptProvider` must stay intact and selectable via `--stub`, `asr.provider = "stub"`, or `SET_MODEL:stub`.
- Model: NVIDIA Parakeet TDT 0.6B v2 INT8 (`sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8`, `model_type = "nemo_transducer"`).
- Single-shot VAD mode: decoding runs once per complete VAD segment (`utteranceFinalized`); `partialTextChanged` remains empty during speech while the UI displays "Listening..." with the live RMS audio-level pulse.
- Zero-padding: final VAD buffers are zero-padded to fixed discrete buckets (32000, 64000, 128000, 240000 samples) to prevent ONNX Runtime arena fragmentation/bloat.
- Memory tuning: glibc `mallopt(M_ARENA_MAX, 2)` and `mallopt(M_MMAP_THRESHOLD, 32 * 1024)` are called at startup in `src/main.cpp` and `tests/tst_live_input.cpp`.
- `CommandEngine` is router-first: `commands.toml` dispatches on the 0 ms deterministic path; only unmatched maths/calculus reaches `LlamaMathTranslator`.
- `MathEvaluator` returns `MathResult` (scalar or vector) and supports `sigmoid`/`sigma`, `relu`, `exp`, `abs`, `softmax([...])`.
- `SymbolicDifferentiator` parses, differentiates and simplifies algebraic expressions (`0.5*(y_hat-y)^2` wrt `y_hat` → `y_hat - y`).
- The deterministic fallback must keep passing with `--stub`, without llama.cpp, and without the GGUF.

## Build & Test Commands
- Configure: `cmake -B build -G Ninja -DBUILD_TESTING=ON`
- Build: `cmake --build build`
- Test: `ctest --test-dir build --output-on-failure -LE live`
- Live microphone test: `./build/tests/tst_live_input` (`DEX_TEST_MODE=engine` by default)
- Models: `bash scripts/download-models.sh` (ASR + VAD + GGUF; `--skip-slm` for ASR only)
- Stub smoke test: `./build/dex --stub --socket /tmp/dex.sock`
- Llama-free build check: `cmake -B build-nollama -G Ninja -DBUILD_TESTING=ON -DCMAKE_DISABLE_FIND_PACKAGE_LlamaCpp=ON`

**Privileges:** Whenever a sudo/elevated command is needed, use `pkexec` instead (e.g. `pkexec pacman -S --needed layer-shell-qt`).

## Architecture & Module Boundaries
1. `src/core/` — `Config` (`config.toml` via `toml++`), `AppController` (daemon vs. one-shot state machine, provider lifecycle, fallback to stub), `IpcServer` (`QLocalServer`).
2. `src/engine/` — `CommandEngine`:
   - `TextNormalizer`: lowercase, fillers, spoken-math canonicalization (`"into"` → `*`, `"l.n.e"` → `ln(e)`, `sigma` → `sigmoid`, `y hat` → `y_hat`), maths-only number words, derivative extraction.
   - `MathEvaluator`: recursive-descent deterministic expression parser returning `MathResult` (`+`, `-`, `*`, `/`, `^`, `%`, parentheses, vectors, `ln`, `log`, `sin`, `cos`, `tan`, `sqrt`, `abs`, `sigmoid`, `relu`, `exp`, `softmax`, `pi`, `e`).
   - `SymbolicDifferentiator`: AST + simplifier for `+ - * / ^`, `ln`, `exp`, `sin`, `cos`, `sigmoid`, `sqrt`.
   - `LlamaMathTranslator`: optional `llama.cpp` backend; GBNF-constrained JSON, lazy load, idle unload.
   - `RuleRouter`: regex/prefix matcher (`QRegularExpression`) loading rules from `commands.toml`.
   - `ActionDispatcher`: `QProcess::startDetached` (`$BROWSER`, app launch, shell exec) + clipboard copy (`wl-copy`).
3. `src/provider/`:
   - `ITranscriptProvider` — contract: `start`/`stop`/`setModelProfile`, `partialTextChanged`, `utteranceFinalized`, `stateChanged`, `levelChanged`, `errorTextChanged`.
   - `StubTranscriptProvider` — IPC-driven text injection.
   - `AudioCapture` — Qt Multimedia 16 kHz mono capture + RMS level; producer side of the ring buffer.
   - `SpscRingBuffer` — lock-free single-producer/single-consumer audio queue.
   - `ModelLayout` — Parakeet and Silero VAD file discovery.
   - `SherpaTranscriptProvider` / `SherpaWorker` — VAD loop, single-shot bucketed decode, model lifecycle on the `dex-asr` thread.
   - `ProviderFactory` — config/flag selection and graceful fallback.
4. `qml/` — Floating M3 pill window (`LayerShellQt`), custom M3 token singleton (`Theme.qml`), status pill (with audio-level pulse and "Listening..."), math card, confirmation bar.

## ASR Rules & Gotchas
- Link the sherpa-onnx **C API** (`sherpa-onnx/c-api/c-api.h`). The prebuilt C++ wrapper has a pre-C++11 libstdc++ ABI and will not link.
- Parakeet requires `model_type = "nemo_transducer"`.
- Never touch sherpa-onnx objects from the GUI thread; `SherpaWorker` owns them.
- Single-shot execution: no simulated streaming or partial decodes during speech; final decode runs once when Silero VAD detects speech end.
- Zero-padding: buffer padded up to discrete buckets (32000, 64000, 128000, 240000 samples) to prevent memory arena bloat.
- At speech start, seed the active buffer with ~600 ms pre-roll to avoid clipping the first phoneme.
- If the model bundle is missing/incomplete, log the error and fall back to the stub rather than crashing.

## Maths SLM Rules & Gotchas
- `llama.cpp` is **optional**: `cmake/FindLlamaCpp.cmake` defines `DEX_HAVE_LLAMA` only when `llama.h` + `libllama.so` exist. The project must build and pass tests without it.
- `[math_slm]` config: `enabled`, `model_path`, `threads` (1–16), `idle_unload_sec` (0–3600). Load nothing at startup; free the model after the idle window.
- Output must be one compact JSON object: `{"mode":"eval"|"diff","expr":"…"}` (+ `"var"`). Enforce it with the GBNF grammar and stop at the first `}`.
- Keep the few-shot examples as `Input:`/`Output:` pairs in the **user** turn; the transcript is the final `Input:`.
- Prefill the assistant turn with `<think\n</think\n` to suppress reasoning.
- Qwen3.5 is a hybrid attention/recurrent model: create a context per query and free it afterwards. Keep the model memory-mapped.
- Every failure (missing GGUF, load error, invalid JSON, unusable expression) must fall through to the deterministic pipeline. Log via the `dex.slm` category.
- The maths gate is `derivative request || (isMathTrigger && hasMathContent)`; `hasMathContent` needs a digit, function/constant or number word, so "turn the power off" never loads the model.

## Coding Standards
- **C++20:** Use `std::optional`, `std::string_view`, `Q_OBJECT` / `QML_ELEMENT` macros (`qt_add_qml_module`). Avoid raw owning pointers.
- **QML:** Strict typing (`property string`, `required property`), zero business logic in QML—bind directly to C++ `Q_PROPERTY` / `Q_INVOKABLE` models.
- **Security:** Always percent-encode (`QUrl::toPercentEncoding`) URL query slots. Never pass raw user speech to `/bin/sh -c` unless explicitly configured in `commands.toml` with `shell = true`.
- **Context Discipline:** Load only the specific spec in `docs/` needed for your current task:
  - @docs/ARCHITECTURE.md
  - @docs/SPEC_ENGINE.md
  - @docs/UI_M3_SPEC.md
  - @docs/TESTING.md
