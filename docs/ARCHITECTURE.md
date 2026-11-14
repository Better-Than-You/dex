# Dex System Architecture

## 1. Runtime Modes & ASR Profiles
Configured in `~/.config/dex/config.toml`:

- **Provider (`asr.provider`):**
  - `"sherpa"` (default) — live 16 kHz capture → Silero VAD → sherpa-onnx.
  - `"stub"` — no audio; transcripts are injected over the IPC socket. The
    `--stub` command-line flag forces this backend regardless of config.
- **Profiles (`asr.model_profile`):**
  - `"moonshine"` (default) — `sherpa-onnx-moonshine-base-en-quantized`
    (~140 MB, `encoder_model.ort` + `decoder_model_merged.ort`). Fast startup
    (~0.8 s) and low RTF; best for short commands.
  - `"parakeet"` — `sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8` (~650 MB,
    encoder/decoder/joiner). Higher accuracy and slower startup (~2.3 s).
  - Legacy aliases are still accepted: `light`/`balanced` → `moonshine`,
    `accurate` → `parakeet`.
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
   accumulates the active segment, and performs offline decodes.
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
   trailing silence. Partials are produced by periodically re-decoding a tail
   of that buffer ("simulated streaming"; Moonshine last 8 s, Parakeet last
   3 s, Parakeet interval ≥ 900 ms). Each partial decode is padded with 350 ms
   of silence so the offline decoder terminates cleanly instead of looping on
   the last word. This is the technique used by sherpa-onnx's own offline
   examples.
5. **Speech end:** the VAD closes the segment after
   `asr.silence_timeout_ms`; the worker decodes the complete VAD segment,
   emits `utteranceFinalized`, and returns to `Listening`.
6. **Hot switch:** `setModelProfile()` resolves the new bundle, drains stale
   audio, and reloads the recognizer on the worker thread without restarting
   the process.

`ITranscriptProvider` is the only contract downstream code sees:
`partialTextChanged`, `utteranceFinalized`, `stateChanged`, `levelChanged`,
`errorTextChanged`, plus `start`/`stop`/`setModelProfile`.

## 4. Command Pipeline
After `CommandEngine` receives `utteranceFinalized`:

1. `TextNormalizer` → canonical lowercase string, spoken-math canonicalisation.
2. `MathEvaluator` → if valid, emit the expression/result, copy the result via
   `wl-copy`, and show the math card.
3. `RuleRouter` → match the first `commands.toml` rule (named captures).
4. `ConfirmationGate` → auto-execute unless `confirm = true` (rule) or
   `general.confirm_by_default = true`.
5. `ActionDispatcher` → `QProcess::startDetached` (URL via `$BROWSER`, app
   launch, optional shell rule).

## 5. IPC Control & Debug Socket (`/run/user/$UID/dex.sock`)
`QLocalServer` (`Qt6::Network`) handles daemon lifecycle control, stub
injection, and model switching:
- `SHOW` / `HIDE` / `TOGGLE` — overlay visibility
- `PARTIAL:<text>` — inject a partial transcript (useful in stub mode)
- `FINAL:<text>` — inject a finalized utterance
- `SET_MODEL:<moonshine|parakeet|stub>` — hot-switch profile (stub switches the
  stub's recorded profile only)
- `CONFIRM` / `CANCEL` — resolve a pending `confirm = true` action
- `QUIT` — clean daemon shutdown

## 6. Model Storage & Layout
Default base directory: `~/.local/share/dex/models`
(`$XDG_DATA_HOME/dex/models` when set). `ModelLayout` searches the base
directory and known bundle subdirectories for the required files:

- **Moonshine:** `encoder_model.ort`, `decoder_model_merged.ort`, `tokens.txt`.
  `model_type` is left empty so sherpa-onnx auto-detects merged-decoder v2.
- **Parakeet:** `encoder.int8.onnx`, `decoder.int8.onnx`, `joiner.int8.onnx`,
  `tokens.txt`, with `model_type = "nemo_transducer"`.
- **VAD (both):** `silero_vad.onnx`, either beside the bundle or in the base
  directory; `asr.vad_model` can override the path.

`scripts/download-models.sh` downloads the VAD plus both bundles and prints a
required-file checklist. If the configured bundle is missing or incomplete,
`ProviderFactory` returns an error and `AppController` falls back to the stub
and surfaces the error in the UI instead of failing to start.

## 7. Directory Layout
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
- `src/engine/`: `TextNormalizer`, `MathEvaluator`, `RuleRouter`,
  `ActionDispatcher`, `CommandEngine`
- `qml/`: `Main.qml`, `Theme.qml`, `components/StatusPill.qml`,
  `components/MathResultCard.qml`, `components/ConfirmBar.qml`
- `tests/`: `tst_normalizer`, `tst_math`, `tst_router`, `tst_config`,
  `tst_engine`, `tst_ipc`, `tst_provider`, `tst_audio`, `tst_live_input`
  (live microphone, soft-skips without hardware/models/speech)
- `cmake/FindSherpaOnnx.cmake`, `scripts/download-models.sh`

## 8. CMake Requirements
- `find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2
  Multimedia Network Test)`
- `find_package(LayerShellQt REQUIRED)`
- Link `tomlplusplus` (`toml++`).
- `find_package(SherpaOnnx REQUIRED)` via `cmake/FindSherpaOnnx.cmake`; Dex
  links the **C API** (`libsherpa-onnx-c-api.so`, `onnxruntime`) because the
  prebuilt C++ wrapper uses a pre-C++11 libstdc++ ABI. The module adds an
  `-Wl,-rpath` for the detected library directory.
- Register QML types via `qt_add_qml_module(dex URI Dex VERSION 1.0 QML_FILES ...)`.
