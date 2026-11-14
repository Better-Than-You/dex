# Dex IPC Socket Protocol & QtTest Matrix

## 1. Unix Domain Socket (`/run/user/$UID/dex.sock`)
Implemented via `QLocalServer` (`Qt6::Network`), newline-delimited commands:
- `SHOW` / `HIDE` / `TOGGLE` — Overlay visibility in daemon mode
- `PARTIAL:<text>` — Feed simulated live partial transcript
- `FINAL:<text>` — Feed finalized utterance into `CommandEngine`
- `SET_MODEL:<moonshine|parakeet|stub>` — Hot-switch ASR profile
- `CONFIRM` / `CANCEL` — Resolve `Confirming` state
- `QUIT` — Terminate daemon cleanly

Stub mode (`--stub` or `asr.provider = "stub"`) exercises the same socket
without a microphone or model files.

## 2. Required `QtTest` Suites (`tests/`, 8 total)
- `tst_normalizer`:
  - `"Hey Dex, search for Arch Linux Wiki."` -> `"search for arch linux wiki"`
  - `"minus 2 into l.n.e"` -> `"-2 * ln(e)"`
  - `"3.14 plus 2.5"` -> `"3.14 + 2.5"`
- `tst_math`:
  - `"minus 2 multiplied by l.n.e"` -> `-2.0`
  - `"minus 2 into l.n.e"` -> `-2.0`
  - `"10 plus 5 times 2"` -> `20.0`
  - `"sqrt 16 plus 2 power 3"` -> `12.0`
  - Division by zero -> `std::nullopt`
- `tst_router`:
  - `"search C++20 concepts"` -> matches `web_search`, encodes query as `C%2B%2B20%20concepts`
  - `"launch kitty"` -> matches `launch_app`, program `kitty`
  - `"workspace 4"` -> matches `hypr_workspace`, args `["dispatch", "workspace", "4"]`
- `tst_config`: shipped `config.toml`, defaults, `~` expansion, clamping, broken TOML.
- `tst_engine`: normalise -> math/route -> dispatch outcomes.
- `tst_ipc`: control commands, `PARTIAL:`/`FINAL:`, `SET_MODEL:`, buffered lines.
- `tst_provider`: factory/flag selection, aliases, Moonshine/Parakeet layouts, missing-file rejection.
- `tst_audio`: SPSC ring-buffer round-trip, wrap-around, drop count, reset.

## 3. Verification Recipes
- Unit: `ctest --test-dir build --output-on-failure`
- Stub IPC:
  ```sh
  ./build/dex --stub --socket /tmp/dex-stub.sock
  printf 'SHOW\nSET_MODEL:parakeet\nFINAL:minus 2 into l.n.e\nQUIT\n' \
    | socat - UNIX-CONNECT:/tmp/dex-stub.sock
  ```
- Live hot-switch: send `SET_MODEL:parakeet` then `SET_MODEL:moonshine` and look
  for `Dex ASR: loaded ...` in the log.
- End-to-end ASR: route the default source to the speaker monitor, play a
  16 kHz mono sample, and check `Dex ASR: final "..."`. Restore the source
  afterwards. See `docs/TESTING.md` for the full script.
- Debug partials with `QT_LOGGING_RULES='dex.asr.debug=true'`. Keep Qt logs on
  stderr with `QT_ASSUME_STDERR_HAS_CONSOLE=1`.
