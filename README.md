# Dex

Dex is an offline, deterministic voice command launcher for Arch Linux and
Hyprland. It renders a floating Material Design 3 pill above the desktop,
captures 16 kHz mono speech locally, and turns finalised utterances into either
a maths result (copied to the clipboard) or a configured command. No audio or
text ever leaves the machine.

**Architecture:** live microphone capture, Silero VAD, single-shot NVIDIA Parakeet
ASR (`sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8`), a vector/ML-aware maths
evaluator, a symbolic differentiator, and an optional on-demand `llama.cpp`
translator for noisy maths/calculus speech. The text-injection stub remains
available for testing.

Nothing is sent to a network service. Desktop commands and ordinary maths are
handled deterministically; the small local SLM is only paged in for an
unmatched maths/calculus utterance, and every SLM failure falls back to the
deterministic parser.

## Requirements

- C++20 compiler, CMake ≥ 3.21, Ninja
- Qt 6.6+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Multimedia`,
  `Network`, `Test`)
- `layer-shell-qt`
- `toml++`
- `sherpa-onnx` C API + ONNX Runtime (headers and `libsherpa-onnx-c-api.so`)
- Optional: `llama.cpp` (`llama.h`, `libllama.so`) — enables the on-demand
  maths/calculus translator. Dex builds and runs without it.
- PipeWire/PulseAudio input device that supports 16 kHz mono
- `socat` (optional, for the IPC smoke test)
- `wl-copy` at runtime for the maths clipboard result

On Arch:

```sh
pkexec pacman -S --needed qt6-base qt6-declarative qt6-quickcontrols2 \
  qt6-multimedia layer-shell-qt tomlplusplus socat
```

Install the sherpa-onnx prebuilt bundle (or build it) so that
`/usr/local/include/sherpa-onnx/c-api/c-api.h` and
`/usr/local/lib/libsherpa-onnx-c-api.so` exist. `cmake/FindSherpaOnnx.cmake`
adds the right include/library paths and an rpath.

For the maths SLM, build/install `llama.cpp` so that `llama.h` and
`libllama.so` are discoverable (for example under `~/.local` or `/usr/local`;
override with `-DLLAMA_CPP_ROOT=...`). When it is missing, CMake prints
`Dex: llama.cpp not found - maths SLM disabled, deterministic maths only` and
the rest of Dex works unchanged.

## Models

```sh
bash scripts/download-models.sh
```

Downloads into `~/.local/share/dex/models`:

- `silero_vad.onnx`
- `sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8` (single-shot transducer)
- `Qwen3.5-0.8B-Q4_K_M.gguf` (maths SLM; `--skip-slm` to omit it)

Use `--dir=/path/to/models` to choose another base directory. If the ASR bundle
is missing, Dex logs the error and falls back to the stub so the UI still starts.
A missing GGUF only disables the maths SLM.

## Build & test

```sh
cmake -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure -LE live
```

`tst_live_input` (label `live`) drives the real microphone; run it directly:
`./build/tests/tst_live_input`.

## Run

```sh
./build/dex
```

In the default daemon mode the overlay stays hidden until `SHOW`/`TOGGLE` is
received over the socket or the process is sent `SIGUSR1`. Use `--one-shot` to
show the overlay immediately, handle one utterance and exit.

Useful flags:

```
-c, --config <path>     config.toml to load
    --commands <path>   commands.toml to load
    --socket <path>     control socket (default $XDG_RUNTIME_DIR/dex.sock)
-1, --one-shot          run without the daemon lifecycle
    --no-ipc            do not start the control socket
    --stub              use text injection instead of live audio/ASR
```

When no config is found in `~/.config/dex/`, the development copy in `config/`
is used automatically.

### Hyprland layer rules

Dex sets its layer-shell scope to `dex`. Disable compositor blur, shadows and
layer animations for that scope so Hyprland does not draw a rectangular
blur/shadow/animating surface behind the pill's rounded corners:

```ini
layerrule = noblur, dex
layerrule = noshadow, dex
layerrule = noanim, dex
```

Add those lines to `~/.config/hypr/hyprland.conf` (or a sourced config) and
reload Hyprland. The rounded pill itself keeps its Material 3 surface; only the
compositor's outer blur/shadow rectangle is removed.

## Control socket

Newline-delimited commands on `$XDG_RUNTIME_DIR/dex.sock`:

```
SHOW | HIDE | TOGGLE | CONFIRM | CANCEL | QUIT
PARTIAL:<text>
FINAL:<text>
SET_MODEL:parakeet|stub
```

Examples:

```sh
printf 'SET_MODEL:parakeet\n' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/dex.sock
printf 'FINAL:minus 2 into l.n.e\n' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/dex.sock
printf 'FINAL:search hyprland layer shell\n' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/dex.sock
```

A scripted smoke test lives at
`.opencode/skills/dex-dev/scripts/ipc-smoke.sh`.

## Configuration

`config/config.toml` documents the defaults:

- `[general]` — `daemon`, `confirm_by_default`, `auto_hide_ms`
- `[asr]` — `provider` (`sherpa`/`stub`), `model_dir`, `vad_model`,
  `threads`, `silence_timeout_ms`, `vad_threshold`
- `[math_slm]` — `enabled`, `model_path`, `threads`, `idle_unload_sec`
  (0 keeps the model resident; otherwise it is freed after that many seconds)
- `[ui]` — `anchor`, `margin_top`, `width`

`config/commands.toml` contains ordered rules. Patterns are Qt regular
expressions with named capture groups; `%{name}` placeholders are substituted
into `exec`/`args`. URL rules percent-encode captured slots before substitution.
A rule may set `confirm = true` to force the confirmation gate; `shell = true`
opts a rule into `/bin/sh -c` execution (never enabled by default).

## Layout

```
src/core/      Config, AppController, IpcServer
src/engine/    TextNormalizer, MathEvaluator, SymbolicDifferentiator,
               LlamaMathTranslator, RuleRouter, ActionDispatcher, CommandEngine
src/provider/  ITranscriptProvider, StubTranscriptProvider, AudioCapture,
               SpscRingBuffer, ModelLayout, SherpaTranscriptProvider, ProviderFactory
qml/           Main.qml, Theme.qml, components/
tests/         QtTest suites (normalizer, maths, router, config, engine, IPC,
               provider, audio, live input)
cmake/         FindSherpaOnnx.cmake, FindLlamaCpp.cmake
scripts/       download-models.sh
```

## ASR details

- Capture: 16 kHz mono `float` (falls back to `int16` if the device requires
  it), pushed into a lock-free SPSC ring buffer.
- VAD: Silero (`silero_vad.onnx`) with a configurable threshold and silence
  timeout. ~600 ms of pre-roll is kept so the first phoneme is not clipped.
- Recognition: single-shot NVIDIA Parakeet TDT 0.6B v2 INT8 on a dedicated
  `dex-asr` thread. No simulated streaming or intermediate partial decodes;
  SherpaOnnx offline recognizer runs once per complete VAD segment
  (`utteranceFinalized`). The UI shows "Listening..." with the live RMS pulse.
- Memory & Tensor Optimization: final audio buffers are zero-padded to fixed
  discrete buckets (32000, 64000, 128000, 240000 samples) so ONNX Runtime reuses
  tensor shapes without arena bloat. Under glibc, `mallopt(M_ARENA_MAX, 2)` and
  `mallopt(M_MMAP_THRESHOLD, 32 * 1024)` are applied at startup.

## Maths pipeline

- Router first: `commands.toml` matches dispatch on the deterministic fast path;
  the SLM is never consulted for a desktop command.
- Deterministic maths: `TextNormalizer` → `MathEvaluator` (scalars, vectors,
  `sigmoid`/`sigma`, `relu`, `exp`, `softmax`) or `SymbolicDifferentiator`
  (`0.5*(y_hat-y)^2` wrt `y_hat` → `y_hat - y`).
- Noisy maths/calculus only: the LLM translates the transcript into one JSON
  command (`{"mode":"eval|diff",...}`), constrained by a GBNF grammar. The
  model loads on the first such query and is freed after `idle_unload_sec`.
- Any failure (no build support, no GGUF, bad JSON, unusable expression) falls
  back to the deterministic path.
- Debug the SLM with `QT_LOGGING_RULES='dex.slm.debug=true'`.

## Current scope

- Single-shot ASR: Silero VAD closes the utterance, then Parakeet decodes it
  once. There are no streaming partial transcripts; while you speak the pill
  shows `"Listening…"` with the live RMS pulse.
- No microphone capture in stub mode; no network calls at all.
- Desktop intent parsing, routing, ordinary maths and differentiation are
  deterministic.
- The optional maths/calculus SLM only rewrites noisy maths/calculus speech; it
  never handles desktop commands and every failure falls back to the
  deterministic path.
- ASR is behind `ITranscriptProvider`; the stub and sherpa backends are
  interchangeable.
