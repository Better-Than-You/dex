---
name: dex-dev
description: Architect, implement, test, and debug Dex — a C++20/Qt6/QML Material 3 voice command launcher for Arch Linux and Hyprland using layer-shell-qt, toml++, recursive-descent spoken math evaluation, optional on-demand llama.cpp maths translation, sherpa-onnx ASR, Silero VAD, and Unix socket IPC. Use whenever working on Dex C++20 modules, QML Material 3 UI, TOML command routing, spoken math/calculus parsing, the maths SLM, live audio/ASR, or QtTest/IPC verification.
---

# Dex Development Skill

**Dex** is an offline voice command launcher for Arch Linux / Hyprland (`C++20`, `Qt6`, `QML`, `layer-shell-qt`, `toml++`, `sherpa-onnx`, Silero VAD, optional `llama.cpp`).
**Strict Rule:** no cloud services and no chat-style LLM in the command path. Desktop routing, spoken-math normalisation, evaluation and differentiation are deterministic; the optional 0.8B SLM only rewrites noisy maths/calculus speech into a JSON command on demand.

## Current Architecture: Single-Shot Parakeet ASR + Maths/Calculus SLM
- `SherpaTranscriptProvider`: 16 kHz capture → Silero VAD → single-shot NVIDIA Parakeet TDT 0.6B v2 INT8 (`sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8`, `model_type = "nemo_transducer"`).
- Single-shot VAD mode: decoding runs once per complete VAD utterance (`utteranceFinalized`). No simulated-streaming partial decodes; `partialTextChanged` remains empty during speech while the UI displays "Listening..." with the live RMS audio-level pulse.
- Tensor bucket padding: final VAD buffers are zero-padded to fixed discrete buckets (32000, 64000, 128000, 240000 samples) so ONNX Runtime reuses fixed tensor shapes without arena bloat.
- Memory arena tuning: calls glibc `mallopt(M_ARENA_MAX, 2)` and `mallopt(M_MMAP_THRESHOLD, 32 * 1024)` at startup in `src/main.cpp` and `tests/tst_live_input.cpp`.
- `StubTranscriptProvider`: headless text-injection backend intact and selectable via `--stub`, `asr.provider = "stub"`, or `SET_MODEL:stub`.
- `LlamaMathTranslator`: on-demand `Qwen3.5-0.8B-Q4_K_M` via `llama.cpp`, GBNF-constrained JSON, cold start, idle unload.
- `MathEvaluator`: returns `MathResult` (scalar **or** vector) with `sigmoid`/`sigma`, `relu`, `exp`, `softmax([...])`, `abs`.
- `SymbolicDifferentiator`: AST + simplifier, e.g. `0.5*(y_hat-y)^2` wrt `y_hat` -> `y_hat - y`.
- `CommandEngine` is **router-first**: `commands.toml` dispatches on the deterministic fast path; the SLM is only consulted for unmatched maths/calculus.
- `scripts/download-models.sh`: downloads only `silero_vad.onnx`, `sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8`, and `Qwen3.5-0.8B-Q4_K_M.gguf` (`--skip-slm` to omit).
- Deterministic fallback must keep working with `--stub`, without llama.cpp, and without the GGUF.

## Build & Verify Commands
- Configure: `cmake -B build -G Ninja -DBUILD_TESTING=ON`
- Build: `cmake --build build`
- Unit tests: `ctest --test-dir build --output-on-failure -LE live` (8 suites)
- Live microphone test: `./build/tests/tst_live_input` (engine mode by default)
- Download models: `bash scripts/download-models.sh` (`--skip-slm` for ASR only)
- Stub smoke test: `./build/dex --stub --socket /tmp/dex-stub.sock`
- IPC smoke test: `bash .opencode/skills/dex-dev/scripts/ipc-smoke.sh`
- Prove the llama-free build: `cmake -B build-nollama -G Ninja -DBUILD_TESTING=ON -DCMAKE_DISABLE_FIND_PACKAGE_LlamaCpp=ON`

## Privilege Escalation
- Whenever elevated privileges are needed (installing packages, writing to system paths), use `pkexec` — never `sudo`.
  Example: `pkexec pacman -S --needed --noconfirm layer-shell-qt`

## Module Layout & Boundaries
1. `src/core/` — `Config` (`toml++` loader for `config.toml`), `AppController` (daemon vs. one-shot state machine, provider lifecycle + stub fallback, owns the maths translator), `IpcServer` (`QLocalServer`).
2. `src/engine/` — `CommandEngine`:
   - `TextNormalizer`: fillers, spoken-math canonicalization (`"into"` -> `*`, `"l.n.e"` -> `ln(e)`, `sigma` -> `sigmoid`, `y hat` -> `y_hat`), decimal-safe punctuation stripping, maths-only number words, derivative extraction.
   - `MathEvaluator`: C++20 recursive-descent parser returning `MathResult` (`+ - * / ^ %`, parentheses, vectors, `ln`, `log`, `sin`, `cos`, `tan`, `sqrt`, `abs`, `sigmoid`, `relu`, `exp`, `softmax`, `pi`, `e`; unary minus; implicit multiplication).
   - `SymbolicDifferentiator`: parse/differentiate/simplify/render algebraic expressions.
   - `LlamaMathTranslator`: optional `llama.cpp` backend, JSON command output, lazy load + idle unload.
   - `RuleRouter`: top-to-bottom `QRegularExpression` matching against `commands.toml` with named slot extraction (`%{query}`, `%{app}`, `%{num}`).
   - `ActionDispatcher`: `QProcess::startDetached` (`$BROWSER` fallback `xdg-open`, app launch) + `wl-copy` clipboard integration.
3. `src/provider/` — `ITranscriptProvider`; `StubTranscriptProvider`; `AudioCapture`; `SpscRingBuffer`; `ModelLayout`; `SherpaTranscriptProvider`/`SherpaWorker`; `ProviderFactory`.
44: `qml/` — `Main.qml` (`LayerShellQt` overlay), `Theme.qml` (M3 dark tonal singleton), `components/` (`StatusPill.qml` with audio-level pulse and "Listening...", `MathResultCard.qml`, `ConfirmBar.qml`).

## ASR Gotchas
- Link `sherpa-onnx/c-api/c-api.h` + `libsherpa-onnx-c-api.so`. The prebuilt C++ wrapper has a pre-C++11 libstdc++ ABI and fails to link.
- Parakeet requires `model_type = "nemo_transducer"`.
- Never touch sherpa objects from the GUI thread. All decoding happens on `SherpaWorker`.
- Single-shot offline decoding: decode once per complete VAD utterance (`utteranceFinalized`). No simulated-streaming partial decodes; `partialTextChanged` stays empty during speech while UI shows "Listening..." with live RMS audio-level pulse.
- Zero-pad final VAD buffer to fixed discrete buckets (32000, 64000, 128000, 240000 samples) before decoding to avoid ONNX Runtime arena bloat.
- Under glibc, set `mallopt(M_ARENA_MAX, 2)` and `mallopt(M_MMAP_THRESHOLD, 32 * 1024)` at startup in `main.cpp` and `tst_live_input.cpp`.
- At speech start, seed the active buffer with ~600 ms pre-roll to avoid clipping the first phoneme.
- If the model bundle is missing, fall back to the stub and surface the error rather than aborting.

## SLM Gotchas
- `llama.cpp` is optional at build time (`cmake/FindLlamaCpp.cmake`, `DEX_HAVE_LLAMA`). Never make it a hard dependency.
- Load the model only on the first maths query; `math_slm.idle_unload_sec` must return idle memory to zero.
- The GBNF grammar is what guarantees JSON. Do not relax it, and stop decoding at the first `}`.
- Qwen3.5 is hybrid (attention + recurrent): build a fresh context per query. Reusing a cleared context was verified to be fine on CPU, but a per-query context is the safe default.
- Keep the few-shot examples in the **user** turn as `Input:`/`Output:` pairs.
- Prefill the assistant turn with `<think\n</think\n` to suppress reasoning.
- Every failure path must fall back to the deterministic normalizer/evaluator. Log via `dex.slm`; debug with `QT_LOGGING_RULES='dex.slm.debug=true'`.

## Coding & Security Rules
- **C++20 / Qt6:** Use `Q_OBJECT` / `QML_ELEMENT` with `qt_add_qml_module(dex URI Dex VERSION 1.0 ...)`. Prefer `std::optional`, `std::string_view`, and smart pointers.
- **QML:** Strict property types (`property string`, `required property`). Keep all parsing/routing in C++; QML only binds to `AppController` properties/signals.
- **Injection Safety:** Always percent-encode URL query slots via `QUrl::toPercentEncoding()`. Never invoke `/bin/sh -c` unless explicitly flagged in `commands.toml`.

## On-Demand Reference Files
Load only the reference file matching the active subtask to keep context minimal:
- `references/engine-math-rules.md` — TOML schemas, pipeline order, normalizer rules, evaluator/differentiator semantics, SLM prompt/grammar, confirmation gate.
- `references/ui-m3-hyprland.md` — `LayerShellQt` window flags, keyboard interactivity switching, M3 color/shape tokens, QML states.
- `references/ipc-and-testing.md` — Unix socket protocol specification, `QtTest` matrix, stub and end-to-end ASR verification.
