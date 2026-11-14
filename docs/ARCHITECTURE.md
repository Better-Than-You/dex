# Dex System Architecture

## 1. Runtime Modes & ASR Configuration
Configured in `~/.config/dex/config.toml`:

- **Provider (`asr.provider`):**
  - `"sherpa"` (default) — live 16 kHz capture → Silero VAD → single-shot NVIDIA Parakeet.
  - `"stub"` — no audio; transcripts are injected over the IPC socket. The
    `--stub` command-line flag forces this backend regardless of config.
- **Model:**
  - NVIDIA Parakeet TDT 0.6B v2 INT8 (`sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8`,
    `encoder.int8.onnx`, `decoder.int8.onnx`, `joiner.int8.onnx`, `tokens.txt`,
    `model_type = "nemo_transducer"`).
  - Profile switching is removed; the single live model is Parakeet (with `stub`
    retained for headless testing).
- **Maths SLM (`math_slm`):**
  - `enabled` (default `true`), `model_path` (default
    `<model dir>/Qwen3.5-0.8B-Q4_K_M.gguf`), `threads` (1–16, default 4) and
    `idle_unload_sec` (0–3600, default 60).
  - Disabled, missing GGUF, or a build without `llama.cpp` simply means maths
    queries take the deterministic path.
- **Lifecycle mode (`daemon = true` default):**
  - `daemon = true`: runs persistently, keeps the model warm in RAM, hides the
    overlay until triggered (`SIGUSR1`, IPC `SHOW` / `TOGGLE`, or speech if
    already shown).
  - `daemon = false`: one-shot mode — opens the overlay immediately, processes
    one utterance (or manual close via `Esc`), and exits.

## 2. Threading Model
Dex keeps the real-time audio callback and the heavy inference path apart:

1. **GUI thread** — `AudioCapture` owns `QAudioSource`; Qt Multimedia delivers
   `readyRead` here. Samples are converted to mono `float` and pushed into a
   lock-free SPSC ring buffer (`SpscRingBuffer<float, 1 << 16>`, ~4 s). The same
   thread computes a smoothed RMS level (`levelChanged`) for the QML pulse.
2. **Worker thread** (`dex-asr`) — `SherpaWorker` owns the sherpa-onnx
   recognizer and Silero VAD. A 30 ms timer drains the ring buffer, runs VAD,
   accumulates the active segment, and performs single-shot offline decodes.
3. **GUI thread** — signals from the worker are queued back to
   `AppController`, which updates QML. No sherpa object is ever touched from
   the UI thread.

The producer never blocks: if inference stalls, the ring buffer drops the
newest samples and the worker logs the drop count.

## 3. ASR Data Flow
1. `AudioCapture` pushes 16 kHz mono `float` samples into the ring buffer and
   emits a smoothed level at ~25 Hz.
2. `SherpaWorker::tick()` drains the buffer and feeds the VAD.
3. **Speech start:** the worker seeds the active buffer with ~600 ms of
   pre-roll held before detection, so the first phoneme is not clipped.
4. **During speech:** the active buffer accumulates VAD-positive audio plus
   trailing silence. There are no live simulated-streaming partial decodes;
   `partialTextChanged` remains empty, and the UI displays "Listening..." with
   the live RMS audio-level pulse.
5. **Speech end:** the VAD closes the segment after
   `asr.silence_timeout_ms`. The complete buffer is zero-padded up to fixed
   discrete buckets (32000, 64000, 128000, or 240000 samples) so ONNX Runtime
   reuses fixed tensor shapes without arena bloat. Under glibc, `mallopt(M_ARENA_MAX, 2)`
   and `mallopt(M_MMAP_THRESHOLD, 32 * 1024)` are applied at startup.
6. The worker decodes once via `SherpaOnnxDecodeOfflineStream`, emits
   `utteranceFinalized`, and returns to `Listening`.

`ITranscriptProvider` is the only contract downstream code sees:
`partialTextChanged`, `utteranceFinalized`, `stateChanged`, `levelChanged`,
`errorTextChanged`, plus `start`/`stop`/`setModelProfile`.

## 4. Command Pipeline
After `CommandEngine` receives `utteranceFinalized`:

1. `TextNormalizer` → canonical lowercase string, filler removal, spoken-math
   canonicalisation, maths-context number words.
2. `RuleRouter` → `commands.toml` rules run **first**. A matched desktop
   command dispatches on the 0 ms path and never touches the SLM.
3. `MathEvaluator::isMathTrigger` + `hasMathContent` /
   `TextNormalizer::derivative` → decide whether the utterance is maths or
   calculus at all. Ordinary phrases that merely contain a trigger word
   ("turn the power off") stop here.
4. **Optional SLM** — `LlamaMathTranslator` turns the noisy transcript into
   `{"mode":"eval|diff","expr":"…","var":"…"}`. `eval` goes through
   `MathEvaluator`; `diff` goes through `SymbolicDifferentiator`.
5. **Deterministic fallback** — if the SLM is disabled/unavailable/failed, the
   same utterance is normalised and evaluated by `MathEvaluator` (or
   `SymbolicDifferentiator` for derivative requests).
6. `ConfirmationGate` → auto-execute unless `confirm = true` (rule) or
   `general.confirm_by_default = true`.
7. `ActionDispatcher` → `QProcess::startDetached` (URL via `$BROWSER`, app
   launch, optional shell rule). Maths results are copied via `wl-copy` and
   shown in `MathResultCard`.

`MathResult` is either scalar or vector; scalars format with 6 significant
digits, vectors (softmax) truncate to 4 decimals, e.g.
`[0.7018, 0.2853, 0.0128]`.

## 5. Maths SLM Lifecycle
`LlamaMathTranslator` (`src/engine/`) is an **optional** front-end for noisy
maths and calculus speech. It is not an intent parser: the router still owns
desktop commands, and every SLM failure falls back to the deterministic path.

- **Build:** `cmake/FindLlamaCpp.cmake` locates `llama.h` and `libllama.so`
  (hints: `LLAMA_CPP_ROOT`, `~/.local`, `/usr/local`, `/usr`). When found,
  `DEX_HAVE_LLAMA=1` and `dex_core` links `LlamaCpp::LlamaCpp`; otherwise the
  translator compiles to unavailable stubs.
- **Runtime:** `available()` needs `math_slm.enabled`, a configured
  `model_path`, and an existing GGUF. Nothing is loaded at startup.
- **Cold start:** the first maths query loads the model (`~0.3–1.5 s`) and
  reports the timing through the `dex.slm` logging category.
- **Prompt:** system rules plus `Input:`/`Output:` few-shot pairs in the user
  turn; the transcript is the final `Input:`. The assistant turn is pre-filled
  with `<think\n</think\n` so Qwen 3.5 skips its reasoning block.
- **Grammar:** a GBNF grammar forces exactly one compact object
  (`{"mode":"eval|diff","expr":"…"}` plus optional `"var"`), and generation
  stops at the first `}`.
- **Context:** Qwen3.5 is a hybrid attention/recurrent model, so a fresh
  `llama_context` is created per query and freed when the query ends; only the
  memory-mapped model persists.
- **Idle unload:** `math_slm.idle_unload_sec` frees the model so idle RSS
  returns to zero. `0` keeps it resident.
- **Failure:** a missing model, load error, malformed JSON, or an expression the
  evaluator rejects all fall through to `TextNormalizer` + `MathEvaluator` /
  `SymbolicDifferentiator`.

## 6. IPC Control & Debug Socket (`/run/user/$UID/dex.sock`)
`QLocalServer` (`Qt6::Network`) handles daemon lifecycle control, stub
injection, and model switching:
- `SHOW` / `HIDE` / `TOGGLE` — overlay visibility
- `PARTIAL:<text>` — inject a partial transcript (useful in stub mode)
- `FINAL:<text>` — inject a finalized utterance
- `SET_MODEL:<moonshine|parakeet|stub>` — hot-switch profile (stub switches the
  stub's recorded profile only)
- `CONFIRM` / `CANCEL` — resolve a pending `confirm = true` action
- `QUIT` — clean daemon shutdown

## 7. Model Storage & Layout
Default base directory: `~/.local/share/dex/models`
(`$XDG_DATA_HOME/dex/models` when set). `ModelLayout` searches the base
directory and known bundle subdirectories for the required files:

- **Moonshine:** `encoder_model.ort`, `decoder_model_merged.ort`, `tokens.txt`.
  `model_type` is left empty so sherpa-onnx auto-detects merged-decoder v2.
- **Parakeet:** `encoder.int8.onnx`, `decoder.int8.onnx`, `joiner.int8.onnx`,
  `tokens.txt`, with `model_type = "nemo_transducer"`.
- **VAD (both):** `silero_vad.onnx`, either beside the bundle or in the base
  directory; `asr.vad_model` can override the path.
- **Maths SLM:** `Qwen3.5-0.8B-Q4_K_M.gguf` (about 510 MB), located via
  `math_slm.model_path`; the default is the base directory above.

`scripts/download-models.sh` downloads the VAD, both ASR bundles and the GGUF
(`--skip-slm` skips the GGUF) and prints a required-file checklist. If the
configured ASR bundle is missing or incomplete, `ProviderFactory` returns an
error and `AppController` falls back to the stub and surfaces the error in the
UI instead of failing to start. A missing GGUF only disables the maths SLM.

## 8. Directory Layout
- `src/main.cpp`
- `src/core/`: `Config.hpp/.cpp`, `IpcServer.hpp/.cpp`, `AppController.hpp/.cpp`
- `src/provider/`:
  - `ITranscriptProvider.hpp`
  - `StubTranscriptProvider.hpp/.cpp`
  - `AudioCapture.hpp/.cpp`
  - `SpscRingBuffer.hpp`
  - `ModelLayout.hpp/.cpp`
  - `SherpaTranscriptProvider.hpp/.cpp`
  - `ProviderFactory.hpp/.cpp`
- `src/engine/`: `TextNormalizer`, `MathEvaluator`, `SymbolicDifferentiator`,
  `LlamaMathTranslator`, `RuleRouter`, `ActionDispatcher`, `CommandEngine`
- `qml/`: `Main.qml`, `Theme.qml`, `components/StatusPill.qml`,
  `components/MathResultCard.qml`, `components/ConfirmBar.qml`
- `tests/`: `tst_normalizer`, `tst_math`, `tst_router`, `tst_config`,
  `tst_engine`, `tst_ipc`, `tst_provider`, `tst_audio`, `tst_live_input`
  (live microphone, soft-skips without hardware/models/speech)
- `cmake/FindSherpaOnnx.cmake`, `cmake/FindLlamaCpp.cmake`,
  `scripts/download-models.sh`

## 9. CMake Requirements
- `find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2
  Multimedia Network Test)`
- `find_package(LayerShellQt REQUIRED)`
- Link `tomlplusplus` (`toml++`).
- `find_package(SherpaOnnx REQUIRED)` via `cmake/FindSherpaOnnx.cmake`; Dex
  links the **C API** (`libsherpa-onnx-c-api.so`, `onnxruntime`) because the
  prebuilt C++ wrapper uses a pre-C++11 libstdc++ ABI. The module adds an
  `-Wl,-rpath` for the detected library directory.
- `find_package(LlamaCpp QUIET)` via `cmake/FindLlamaCpp.cmake`. Found →
  `DEX_HAVE_LLAMA=1` and link `LlamaCpp::LlamaCpp`; not found → deterministic
  maths only (the build is still fully functional).
- Register QML types via `qt_add_qml_module(dex URI Dex VERSION 1.0 QML_FILES ...)`.
