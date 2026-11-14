# Dex

Dex is an offline, deterministic voice command launcher for Arch Linux and
Hyprland. It renders a floating Material Design 3 pill above the desktop, turns
finalised utterances into either a maths result (copied to the clipboard) or a
configured command, and never sends audio or text to a network service.

Phase 1 ships the full UI shell and the deterministic engine. Audio capture and
`whisper.cpp` are intentionally not wired in yet; transcripts arrive through an
abstraction (`ITranscriptProvider`) and, for now, a Unix socket stub.

## Requirements

- C++20 compiler, CMake ≥ 3.21, Ninja
- Qt 6.6+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Network`, `Test`)
- `layer-shell-qt`
- `toml++`
- `socat` (optional, for the IPC smoke test)
- `wl-copy` at runtime for the maths clipboard result

On Arch:

```sh
pkexec pacman -S --needed qt6-base qt6-declarative qt6-quickcontrols2 layer-shell-qt tomlplusplus socat
```

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
```

When no config is found in `~/.config/dex/`, the development copy in `config/`
is used automatically.

## Control socket

Newline-delimited commands on `$XDG_RUNTIME_DIR/dex.sock`:

```
SHOW | HIDE | TOGGLE | CONFIRM | CANCEL | QUIT
PARTIAL:<text>
FINAL:<text>
```

Examples:

```sh
printf 'FINAL:minus 2 into l.n.e\n' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/dex.sock
printf 'FINAL:search hyprland layer shell\n' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/dex.sock
```

A scripted smoke test lives at
`.opencode/skills/dex-dev/scripts/ipc-smoke.sh`.

## Configuration

`config/config.toml` documents the defaults:

- `[general]` — `daemon`, `confirm_by_default`, `auto_hide_ms`
- `[asr]` — Phase 2 placeholders (`model_profile`, `threads`, `silence_timeout_ms`)
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
src/provider/  ITranscriptProvider, StubTranscriptProvider
qml/           Main.qml, Theme.qml, components/
tests/         QtTest suites (normalizer, maths, router, config, engine, IPC)
```

## Phase 1 scope

- No microphone capture, no `whisper.cpp`, no network calls.
- All intent parsing, maths and routing is deterministic.
- ASR is behind `ITranscriptProvider` so Phase 2 can swap the stub for
  `AudioCapture + WhisperWorker` without touching the engine or QML.
