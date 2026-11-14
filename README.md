# Dex

Dex is an offline, deterministic voice command launcher for Arch Linux and
Hyprland. It renders a floating Material Design 3 pill above the desktop,
captures 16 kHz mono speech locally, and turns finalised utterances into either
a maths result (copied to the clipboard) or a configured command. No audio or
text ever leaves the machine.

**Phase 2 is implemented:** live microphone capture, Silero VAD, and a
dual-profile sherpa-onnx ASR backend (`moonshine` default, `parakeet`
alternative). The Phase 1 text-injection stub remains available for testing.
There are no LLMs anywhere in the pipeline.

## Requirements

- C++20 compiler, CMake ≥ 3.21, Ninja
- Qt 6.6+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Multimedia`,
  `Network`, `Test`)
- `layer-shell-qt`
- `toml++`
- `sherpa-onnx` C API + ONNX Runtime (headers and `libsherpa-onnx-c-api.so`)
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

## Models

```sh
bash scripts/download-models.sh
```

Downloads into `~/.local/share/dex/models`:

- `silero_vad.onnx`
- `sherpa-onnx-moonshine-base-en-quantized-2026-02-27` (default profile)
- `sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8` (alternative profile)

Use `--dir=/path/to/models` to choose another base directory. If a configured
bundle is missing, Dex logs the error and falls back to the stub so the UI still
starts.

## Build & test

```sh
cmake -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

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

## Control socket

Newline-delimited commands on `$XDG_RUNTIME_DIR/dex.sock`:

```
SHOW | HIDE | TOGGLE | CONFIRM | CANCEL | QUIT
PARTIAL:<text>
FINAL:<text>
SET_MODEL:moonshine|parakeet|stub
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
- `[asr]` — `provider` (`sherpa`/`stub`), `model_profile`
  (`moonshine`/`parakeet`), `model_dir`, `vad_model`, `threads`,
  `silence_timeout_ms`, `partial_interval_ms`, `vad_threshold`
- `[ui]` — `anchor`, `margin_top`, `width`

`config/commands.toml` contains ordered rules. Patterns are Qt regular
expressions with named capture groups; `%{name}` placeholders are substituted
into `exec`/`args`. URL rules percent-encode captured slots before substitution.
A rule may set `confirm = true` to force the confirmation gate; `shell = true`
opts a rule into `/bin/sh -c` execution (never enabled by default).

## Layout

```
src/core/      Config, AppController, IpcServer
src/engine/    TextNormalizer, MathEvaluator, RuleRouter, ActionDispatcher, CommandEngine
src/provider/  ITranscriptProvider, StubTranscriptProvider, AudioCapture,
               SpscRingBuffer, ModelLayout, SherpaTranscriptProvider, ProviderFactory
qml/           Main.qml, Theme.qml, components/
tests/         QtTest suites (normalizer, maths, router, config, engine, IPC, provider, audio)
cmake/         FindSherpaOnnx.cmake
scripts/       download-models.sh
```

## ASR details

- Capture: 16 kHz mono `float` (falls back to `int16` if the device requires
  it), pushed into a lock-free SPSC ring buffer.
- VAD: Silero (`silero_vad.onnx`) with a configurable threshold and silence
  timeout. ~600 ms of pre-roll is kept so the first phoneme is not clipped.
- Recognition: sherpa-onnx offline models on a dedicated `dex-asr` thread.
  Partials re-decode a tail of the active VAD buffer ("simulated streaming");
  finals decode the complete VAD segment.
- Hot switching: `SET_MODEL:` or `asr.model_profile` reloads the recognizer
  without restarting Dex.

## Phase scope

- No microphone capture in stub mode; no network calls at all.
- All intent parsing, maths and routing is deterministic.
- ASR is behind `ITranscriptProvider`; the stub and sherpa backends are
  interchangeable.
